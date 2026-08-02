#include "meter.h"

// Smoothing window of the DC offset's exponential moving average (EMA;
// emonLib uses 1024). Larger means a slower, steadier estimate; it is not
// specific to the installation, so it is a module constant rather than a
// MeterConfig field.
static constexpr float kDcFilterWindow = 1024.0f;

Meter::Meter(const MeterConfig& config)
    : config_(config), dc_offset_(0.0f), seeded_(false) {}

float Meter::addSample(float volts) {
  updateDcOffset(volts);
  return volts - dc_offset_;
}

void Meter::updateDcOffset(float volts) {
  // Seed the estimate with the first sample so it starts near the real offset
  // instead of drifting up from zero.
  if (!seeded_) {
    dc_offset_ = volts;
    seeded_ = true;
  } else {
    dc_offset_ += (volts - dc_offset_) / kDcFilterWindow;
  }
}

float Meter::dcOffset() const { return dc_offset_; }
