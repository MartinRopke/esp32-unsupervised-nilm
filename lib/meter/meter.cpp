#include "meter.h"

#include <cmath>

// Smoothing window of the DC offset's exponential moving average (EMA;
// emonLib uses 1024). Larger means a slower, steadier estimate; it is not
// specific to the installation, so it is a module constant rather than a
// MeterConfig field.
static constexpr float kDcFilterWindow = 1024.0f;

static constexpr float kMicrosPerSecond = 1e6f;

Meter::Meter(const MeterConfig& config) : config_(config), dc_offset_(0.0f), seeded_(false) {}

SampleResult Meter::addSample(float burdenVolts, uint32_t timestampMicros) {
  updateDcOffset(burdenVolts);
  float acVolts = burdenVolts - dc_offset_;

  const auto windowMicros = static_cast<uint32_t>(config_.rmsWindowSeconds * kMicrosPerSecond);
  bool closed = rms_window_.accumulate(acVolts, timestampMicros, windowMicros);

  return {acVolts, closed, rms_window_.vRms()};
}

void Meter::updateDcOffset(float burdenVolts) {
  // Seed the estimate with the first sample so it starts near the real offset
  // instead of drifting up from zero.
  if (!seeded_) {
    dc_offset_ = burdenVolts;
    seeded_ = true;
  } else {
    dc_offset_ += (burdenVolts - dc_offset_) / kDcFilterWindow;
  }
}

float Meter::dcOffset() const { return dc_offset_; }

bool Meter::RmsWindow::accumulate(float acVolts, uint32_t timestampMicros, uint32_t windowMicros) {
  if (!started_) {
    start_micros_ = timestampMicros;
    started_ = true;
  }

  sum_squares_ += acVolts * acVolts;
  ++sample_count_;

  // Closed by elapsed time (a multiple of the mains cycle), not by a fixed
  // sample count: 860 SPS does not divide a 60 Hz cycle into a whole number
  // of samples (ADR 0002).
  if (timestampMicros - start_micros_ < windowMicros) {
    return false;
  }

  v_rms_ = sqrtf(sum_squares_ / static_cast<float>(sample_count_));

  sum_squares_ = 0.0f;
  sample_count_ = 0;
  // Rebase to the ideal boundary, not to this closing sample's (later)
  // timestamp, so windows stay aligned to windowMicros multiples of the
  // first sample instead of drifting later by one sample interval each time.
  start_micros_ += windowMicros;

  return true;
}
