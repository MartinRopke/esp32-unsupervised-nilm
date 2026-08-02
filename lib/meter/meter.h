#pragma once

#include <cstdint>

// Pure measurement module for the reading system. No Arduino / Wire / Serial
// dependencies, so it builds and is unit tested in the `native` environment.

// Single install configuration block. All values are specific to the
// installation or appliance and are injected into the Meter (never hardcoded
// in the measurement logic).
struct MeterConfig {
  float mainsVoltage;       // MAINS_VOLTAGE  [V]       e.g. 220
  float calibrationFactor;  // CALIBRATION_FACTOR       empirical CT calibration
  float ctRatio;            // CT_RATIO                 e.g. 2000
  float burdenOhms;         // BURDEN_OHMS    [ohm]     e.g. 22
  float rmsWindowSeconds;   // RMS_WINDOW     [s]       e.g. 1.0
  float tariff;             // TARIFF         [R$/kWh]
};

// Result of feeding one sample to the Meter. `vRms` is only meaningful when
// `windowClosed` is true.
struct SampleResult {
  float centered;
  bool windowClosed;
  float vRms;
};

class Meter {
 public:
  explicit Meter(const MeterConfig& config);

  // Feeds one raw burden voltage sample and its timestamp (micros()). The
  // returned centered value has the DC offset removed. When this sample
  // closes the RMS window (rmsWindowSeconds of config, closed by elapsed
  // time), windowClosed is true and vRms carries the window's burden V_rms.
  SampleResult addSample(float volts, uint32_t timestampMicros);

  // Current exponential moving average (EMA) estimate of the DC offset
  // (bias offset).
  float dcOffset() const;

 private:
  // Exponential moving average (EMA) update step: folds one raw sample into
  // the running DC offset estimate.
  void updateDcOffset(float volts);

  // Accumulates the burden RMS voltage over a time closed window. Owns only
  // the window's accumulator state, so it changes for a single reason
  // (windowing policy) independently of the DC offset filter.
  class RmsWindow {
   public:
    // Feeds one centered sample. Returns true when windowMicros of elapsed
    // time close the window on this call; vRms() then holds the result and
    // the accumulator resets for the next window.
    bool accumulate(float centered, uint32_t timestampMicros, uint32_t windowMicros);

    float vRms() const { return v_rms_; }

   private:
    float sum_squares_ = 0.0f;
    uint32_t sample_count_ = 0;
    uint32_t start_micros_ = 0;
    bool started_ = false;
    float v_rms_ = 0.0f;
  };

  MeterConfig config_;
  float dc_offset_;
  bool seeded_;
  RmsWindow rms_window_;
};
