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

// Result of feeding one sample to the Meter. `vRms` and `iRms` are only
// meaningful when `windowClosed` is true.
struct SampleResult {
  float acVolts;  // burden voltage with the DC offset removed
  bool windowClosed;
  float vRms;  // burden RMS voltage
  float iRms;  // primary RMS current, converted from vRms via config
};

class Meter {
 public:
  explicit Meter(const MeterConfig& config);

  // Feeds one raw burden voltage sample and its timestamp (micros()). The
  // returned acVolts has the DC offset removed. When this sample closes the
  // RMS window (rmsWindowSeconds of config, closed by elapsed time),
  // windowClosed is true and vRms carries the window's burden V_rms.
  SampleResult addSample(float burdenVolts, uint32_t timestampMicros);

  // Current exponential moving average (EMA) estimate of the DC offset
  // (bias offset).
  float dcOffset() const;

 private:
  // Converts a burden RMS voltage into primary RMS current via the
  // calibration chain: I_secondary = vRms / burdenOhms, I_primary =
  // I_secondary * ctRatio, scaled by the single empirical calibrationFactor
  // so it can be tuned without recompiling the logic.
  float toIRms(float vRms) const;

  // Exponential moving average (EMA) estimator of the DC offset (bias
  // offset).
  class DcOffsetFilter {
   public:
    // Feeds one raw sample; returns the updated DC offset estimate. Seeds
    // the estimate with the first sample so it starts near the real offset
    // instead of drifting up from zero.
    float update(float burdenVolts);

    float value() const;

   private:
    float dc_offset_ = 0.0f;
    bool seeded_ = false;
  };

  // Accumulates the burden RMS voltage over a time closed window. Owns only
  // the window's accumulator state, so it changes for a single reason
  // (windowing policy) independently of the DC offset filter.
  class VRmsWindow {
   public:
    explicit VRmsWindow(float windowSeconds);

    // Result of feeding one sample. `vRms` is only meaningful when `closed`
    // is true.
    struct Result {
      bool closed;
      float vRms;
    };

    // Feeds one acVolts sample (burden voltage, DC offset already removed).
    // closed is true when window_micros_ of elapsed time close the window
    // on this call; vRms then carries the window's result and the
    // accumulator resets for the next window.
    Result accumulate(float acVolts, uint32_t timestampMicros);

   private:
    const uint32_t window_micros_;
    float sum_squares_ = 0.0f;
    uint32_t sample_count_ = 0;
    uint32_t start_micros_ = 0;
    bool started_ = false;
  };

  MeterConfig config_;
  DcOffsetFilter dc_offset_filter_;
  VRmsWindow v_rms_window_;
  float v_rms_ = 0.0f;
  float i_rms_ = 0.0f;
};
