#include <Adafruit_ADS1X15.h>
#include <Wire.h>

#include "meter.h"

// ---------------------------------------------------------------------------
// Installation configuration block.
// All constants specific to the installation or appliance live here so the
// firmware is agnostic to where it runs. To use another installation, edit
// only this block.
// ---------------------------------------------------------------------------
static constexpr MeterConfig kConfig = {
    /* mainsVoltage     */ 220.0f,   // [V]      switch to 127.0f on a 127 V installation
    /* calibrationFactor*/ 0.9271f,  //          empirical CT calibration
    /* ctRatio          */ 2000.0f,  //          SCT-013-000 turns ratio
    /* burdenOhms       */ 22.0f,    // [ohm]    burden resistor
    /* rmsWindowSeconds */ 1.0f,     // [s]      RMS window = 60 mains cycles
    /* tariff           */ 0.92f,    // [R$/kWh]
};

Adafruit_ADS1115 ads;
Meter meter(kConfig);

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
  // Continuous mode: the chip samples A0 continuously.
  ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);
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
      Serial.println(result.vRms, 4);
      Serial.print(">irms:");
      Serial.println(result.iRms, 4);
    }
  }
}
