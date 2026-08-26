#include <Adafruit_ADS1X15.h>
#include <Wire.h>

#include "event_clusterer.h"
#include "event_detector.h"
#include "event_merger.h"
#include "meter.h"
#include "session_csv_output.h"
#include "teleplot_output.h"

// ---------------------------------------------------------------------------
// Installation configuration block.
// All constants specific to the installation or appliance live here so the
// firmware is agnostic to where it runs. To use another installation, edit
// only this block.
// ---------------------------------------------------------------------------
// mainsVoltage and calibrationFactor were measured on 19-20 Aug 2026, with a
// combined uncertainty of ~1.7%, every term of which is now bounded by a
// direct measurement.
//
// mainsVoltage is a fixed stand-in for a quantity the system does not
// measure and that varies with load and time of day.
static constexpr MeterConfig kConfig = {
    /* mainsVoltage     */ 236.75f,  // [V]      switch to 127.0f on a 127 V installation
    /* calibrationFactor*/ 1.0092f,  //          measured, ~1.7% uncertainty
    /* ctRatio          */ 2000.0f,  //          SCT-013-000 turns ratio
    /* burdenOhms       */ 22.0f,    // [ohm]    burden resistor
    /* rmsWindowSeconds */ 1.0f,     // [s]      RMS window = 60 mains cycles
    /* tariff           */ 0.92f,    // [R$/kWh]
};

// Threshold and confirmation window from the event-detection literature:
// 30 VA follows the reference-dataset labelling convention (Pereira 2019;
// Rehman et al. 2020); 3 samples merges a switching transient into one
// event (Lu and Li 2020).
static constexpr EventDetectorConfig kEventDetectorConfig = {
    /* thresholdVa               */ 30.0f,  // [VA]
    /* confirmationWindowSamples */ 3,
};

// A transition spanning several sampling intervals reaches the detector as
// 2-4 fragments of one appliance switching, close in time and far apart in
// magnitude; merging them prevents one appliance from being seen as
// several. 5.0 s: one confirmation window (3 s) plus one sample is the
// minimum possible gap between fragments, so 5 s adds one sample of slack.
static constexpr EventMergerConfig kEventMergerConfig = {
    /* mergeWindowSeconds */ 5.0f,  // [s]
};

// epsilonVa: 2 sigma of the pooled within-appliance dispersion measured
// across three appliances (6.2 VA), the smaller of the two candidates a
// sigma-multiple derivation considers, matching the empirical plateau that
// separates the same three appliances into distinct clusters. minPoints:
// 2 x dimensionality (Schubert et al. 2017) raised for declared noise, and
// independently equal to two complete on/off cycles, since an appliance
// seen only once is not distinguishable from an artefact. maxEvents: bounds
// history_ to maxEvents * sizeof(float) = 128 * 4 B = 512 B resident, plus
// O(maxEvents) transient working buffers per call, freed immediately after,
// negligible against the ESP32's RAM and generous against the ~40 events a
// half-hour bench session produces.
static constexpr EventClustererConfig kEventClustererConfig = {
    /* epsilonVa  */ 12.0f,  // [VA]
    /* minPoints  */ 4,
    /* maxEvents  */ 128,
};

// ---------------------------------------------------------------------------
// Session output configuration.
// Independent of the installation block above: toggles which serial
// outputs this build emits, for bench-session control rather than physical
// calibration.
// ---------------------------------------------------------------------------
// Toggle either output off to keep a bench-session serial log free of the
// other's lines (e.g. CSV-only while capturing docs/measurements/ data).
// Compile-time, so flipping one means reflashing.
static constexpr bool kEnableTeleplotOutput = false;
static constexpr bool kEnableCsvOutput = true;

Adafruit_ADS1115 ads;
Meter meter(kConfig);
EventDetector eventDetector(kEventDetectorConfig);
EventMerger eventMerger(kEventMergerConfig);
EventClusterer eventClusterer(kEventClustererConfig);

uint32_t lastSampleMicros{0};

// 1,000,000 us / 860 SPS = ~1163 us per sample.
static constexpr uint32_t sampleIntervalMicros = 1163;

void setup() {
  Serial.begin(921600);

  // t_s comes from micros(), a uint32_t that wraps every 71.6 minutes; a capture longer than
  // that loses alignment between the wrapped rows. Keep captures under 45 minutes, with margin.
  Serial.println("t_s,vrms,irms,power,event,event_t_s,delta_va,direction,cluster,fragments");

  if (!ads.begin()) {
    Serial.println("Failed to start the ADS1115!");
    while (1);
  }

  ads.setDataRate(RATE_ADS1115_860SPS);
  ads.setGain(GAIN_SIXTEEN);
  // Continuous mode: the chip samples the A0-A1 differential continuously.
  ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, /*continuous=*/true);
  // 400 kHz I2C so reads keep up with 860 SPS.
  Wire.setClock(400000);
}

void loop() {
  uint32_t now{micros()};

  if (now - lastSampleMicros >= sampleIntervalMicros) {
    lastSampleMicros = now;

    int16_t raw = ads.getLastConversionResults();
    float burdenVolts = ads.computeVolts(raw);

    // Hand the raw burden sample to the measurement module; it owns the logic.
    SampleResult result = meter.addSample(burdenVolts, now);

    // The detector only ever sees one apparent-power sample per closed window, so it can't
    // run before the window closes; detection stays default (no event) otherwise.
    DetectionResult detection{};
    if (result.windowClosed) {
      detection = eventDetector.addSample(result.apparentPower, now);
    }

    // The merger is fed every closed window: a detected event when there is one, otherwise a
    // tick, so a held fragment isn't stuck waiting for one that never arrives.
    MergeResult merge{};
    if (result.windowClosed) {
      merge = detection.eventDetected ? eventMerger.addEvent(detection.event, now)
                                      : eventMerger.tick(now);
    }

    // The clusterer only sees a released (possibly merged) event, never a raw fragment.
    int32_t clusterId = -1;
    if (merge.eventReady) {
      clusterId = eventClusterer.addEvent(merge.event.magnitudeVa);
    }

    if (kEnableTeleplotOutput) {
      printTeleplotSample(meter.dcOffset(), result.acVolts);
      printTeleplotWindow(result);
      printTeleplotEvent(detection);
    }

    if (kEnableCsvOutput) {
      printSessionCsvRow(now, result, merge, clusterId);
    }
  }
}
