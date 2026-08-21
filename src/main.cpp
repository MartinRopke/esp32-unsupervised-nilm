#include <Adafruit_ADS1X15.h>
#include <Wire.h>

#include "event_detector.h"
#include "meter.h"

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

Adafruit_ADS1115 ads;
Meter meter(kConfig);
EventDetector eventDetector(kEventDetectorConfig);

uint32_t lastSampleMicros{0};

// 1,000,000 us / 860 SPS = ~1163 us per sample.
static constexpr uint32_t sampleIntervalMicros = 1163;

void setup() {
  Serial.begin(921600);

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

    Serial.print(">dc:");
    Serial.println(meter.dcOffset(), 4);
    Serial.print(">ac:");
    Serial.println(result.acVolts, 4);

    if (result.windowClosed) {
      Serial.print(">vrms:");
      Serial.println(result.vRms, 6);
      Serial.print(">irms:");
      Serial.println(result.iRms, 4);
      Serial.print(">power:");
      Serial.println(result.apparentPower, 4);

      const DetectionResult detection = eventDetector.addSample(result.apparentPower, now);
      if (detection.eventDetected) {
        Serial.print(">t_s:");
        Serial.println(detection.event.timestampMicros / 1e6f, 6);
        Serial.print(">delta_va:");
        Serial.println(detection.event.magnitudeVa, 4);
        Serial.print(">direction:");
        Serial.println(detection.event.direction == Direction::kOn ? "on" : "off");
        // Signed +1/-1 so Teleplot can plot the event, since it can't graph the text token above.
        Serial.print(">event:");
        Serial.println(detection.event.direction == Direction::kOn ? 1 : -1);
      }
    }
  }
}
