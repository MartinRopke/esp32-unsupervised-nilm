#include "meter.h"

#include <cmath>

// Smoothing window of the DC offset's exponential moving average (EMA;
// emonLib uses 1024). Larger means a slower, steadier estimate; it is not
// specific to the installation, so it is a module constant rather than a
// MeterConfig field.
static constexpr float kDcFilterWindow = 1024.0f;

static constexpr float kMicrosPerSecond = 1e6f;

Meter::Meter(const MeterConfig& config) : config_(config), v_rms_window_(config.rmsWindowSeconds) {}

SampleResult Meter::addSample(float burdenVolts, uint32_t timestampMicros) {
  float dcOffset = dc_offset_filter_.update(burdenVolts);
  float acVolts = burdenVolts - dcOffset;

  VRmsWindow::Result window = v_rms_window_.accumulate(acVolts, timestampMicros);
  if (window.closed) {
    v_rms_ = window.vRms;
    i_rms_ = toIRms(v_rms_);
  }

  return {acVolts, window.closed, v_rms_, i_rms_};
}

float Meter::toIRms(float vRms) const {
  return vRms / config_.burdenOhms * config_.ctRatio * config_.calibrationFactor;
}

float Meter::dcOffset() const { return dc_offset_filter_.value(); }

float Meter::DcOffsetFilter::update(float burdenVolts) {
  // Seed the estimate with the first sample so it starts near the real offset
  // instead of drifting up from zero.
  if (!seeded_) {
    seeded_ = true;
    dc_offset_ = burdenVolts;
  } else {
    dc_offset_ += (burdenVolts - dc_offset_) / kDcFilterWindow;
  }
  return dc_offset_;
}

float Meter::DcOffsetFilter::value() const { return dc_offset_; }

Meter::VRmsWindow::VRmsWindow(float windowSeconds)
    : window_micros_(static_cast<uint32_t>(windowSeconds * kMicrosPerSecond)) {}

Meter::VRmsWindow::Result Meter::VRmsWindow::accumulate(float acVolts, uint32_t timestampMicros) {
  if (!started_) {
    start_micros_ = timestampMicros;
    started_ = true;
  }

  sum_squares_ += acVolts * acVolts;
  ++sample_count_;

  // Closed by elapsed time (a multiple of the mains cycle), not by a fixed
  // sample count: 860 SPS does not divide a 60 Hz cycle into a whole number
  // of samples.
  if (timestampMicros - start_micros_ < window_micros_) {
    return {false, 0.0f};
  }

  float vRms = sqrtf(sum_squares_ / static_cast<float>(sample_count_));

  sum_squares_ = 0.0f;
  sample_count_ = 0;
  // Rebase to the ideal boundary, not to this closing sample's (later)
  // timestamp, so windows stay aligned to window_micros_ multiples of the
  // first sample instead of drifting later by one sample interval each time.
  start_micros_ += window_micros_;

  return {true, vRms};
}
