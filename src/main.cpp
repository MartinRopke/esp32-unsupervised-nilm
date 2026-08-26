#include <Adafruit_ADS1X15.h>
#include <Wire.h>

#include "cluster_report.h"
#include "cluster_report_output.h"
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
// tariff: PROVISIONAL. No sourced value (a dated utility invoice or an ANEEL table) was on hand
// when ClusterReportConfig was wired up to consume this; 0.92 is a placeholder carried over from
// before this field was read by anything, not a citation. Do not quote a cost figure computed
// from it without replacing this comment with an actual source first.
static constexpr MeterConfig kConfig = {
    /* mainsVoltage     */ 236.75f,  // [V]      switch to 127.0f on a 127 V installation
    /* calibrationFactor*/ 1.0092f,  //          measured, ~1.7% uncertainty
    /* ctRatio          */ 2000.0f,  //          SCT-013-000 turns ratio
    /* burdenOhms       */ 22.0f,    // [ohm]    burden resistor
    /* rmsWindowSeconds */ 1.0f,     // [s]      RMS window = 60 mains cycles
    /* tariff           */ 0.92f,    // [R$/kWh] PROVISIONAL, see comment above
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
// several. 5 samples: one confirmation window (3 s) plus one sample is the
// minimum possible gap between fragments, so 5 samples adds one sample of
// slack. Expressed as a sample count rather than a bare seconds value
// because the dated instants it compares come from a window that closes on
// elapsed time, not a fixed sample count (see EventMergerConfig): a bare
// `mergeWindowSeconds` let jitter of about a millisecond push a genuine pair
// just past a floating-point boundary. sampleIntervalSeconds matches
// kConfig.rmsWindowSeconds, the nominal period between dated instants.
static constexpr EventMergerConfig kEventMergerConfig = {
    /* mergeWindowSamples    */ 5,
    /* sampleIntervalSeconds */ 1.0f,  // [s]
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

// maxEvents mirrors kEventClustererConfig.maxEvents (see ClusterReportConfig's comment on why
// that must hold). tariffReaisPerKwh is kConfig.tariff, PROVISIONAL per the flag above it.
static constexpr ClusterReportConfig kClusterReportConfig = {
    /* maxEvents          */ 128,
    /* tariffReaisPerKwh  */ kConfig.tariff,  // [R$/kWh] PROVISIONAL
};

// Which power-factor category applies to which cluster id is human interpretation (see
// cluster_report.h's file header), not something this firmware infers; it is filled in here, per
// bench run, only once a session's cluster ids are known from its console/CSV log, the same way
// docs/measurements/*-cluster-map.csv is filled in by hand after capture. Empty by default: with
// no assignment, every cluster still reports its unsupervised apparent energy and cost range, no
// point estimate.
static const std::vector<ClusterPowerFactorAssignment> kClusterPowerFactorAssignments = {};

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
ClusterReporter clusterReporter(kClusterReportConfig);

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

    // The clusterer only sees a released (possibly merged) event, never a raw fragment. The
    // report keeps its own (direction, dated instant) history in lockstep, fed the same events
    // in the same order, so it stays index-aligned with the clusterer's history without reading
    // it directly (see ClusterReportConfig's comment in cluster_report.h).
    int32_t clusterId = -1;
    if (merge.eventReady) {
      clusterId = eventClusterer.addEvent(merge.event.magnitudeVa);
      clusterReporter.addEvent(merge.event);
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

  // On-demand report: send 'r' for the console block, 'c' for its CSV form. There is no
  // in-firmware notion of "end of capture" (the board only stops when the operator closes the
  // serial port, which reboots it, see docs/measurements' "reboots on serial reconnect" note):
  // sending 'r'/'c' right before disconnecting is how an operator gets the end-of-capture report
  // this issue's console-output goal describes.
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'r') {
      printClusterReportConsole(clusterReporter.buildReport(eventClusterer.labels(), now),
                                kClusterReportConfig.tariffReaisPerKwh,
                                kClusterPowerFactorAssignments);
    } else if (command == 'c') {
      printClusterReportCsv(clusterReporter.buildReport(eventClusterer.labels(), now),
                            kClusterReportConfig.tariffReaisPerKwh, kClusterPowerFactorAssignments);
    }
  }
}
