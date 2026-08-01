#pragma once

// Pure measurement module for the reading system. No Arduino / Wire / Serial
// dependencies, so it builds and is unit-tested in the `native` environment.

// Single install configuration block. All values are specific to the
// installation or appliance and are injected into the Meter (never hardcoded
// in the measurement logic).
struct MeterConfig {
  float mainsVoltage;       // MAINS_VOLTAGE  [V]      e.g. 220
  float calibrationFactor;  // CALIBRATION_FACTOR       empirical CT calibration
  float ctRatio;            // CT_RATIO                 e.g. 2000
  float burdenOhms;         // BURDEN_OHMS    [ohm]     e.g. 22
  float rmsWindowSeconds;   // RMS_WINDOW     [s]       e.g. 1.0
  float tariff;             // TARIFF         [R$/kWh]
};

class Meter {
 public:
  explicit Meter(const MeterConfig& config);

  // Feeds one raw burden voltage sample and returns the centered sample with
  // the offset removed. Updates the running DC level estimate.
  float addSample(float volts);

  // Current moving average estimate of the DC level (bias offset).
  float dcLevel() const;

 private:
  MeterConfig config_;
  float dc_;
  bool seeded_;
};
