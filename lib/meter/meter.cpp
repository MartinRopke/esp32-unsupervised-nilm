#include "meter.h"

// Smoothing window of the DC offset moving average (emonLib uses 1024). Larger
// means a slower, steadier estimate; it is not specific to the installation,
// so it is a module constant rather than a MeterConfig field.
static constexpr float kDcFilterWindow = 1024.0f;

Meter::Meter(const MeterConfig& config) : config_(config), dc_(0.0f), seeded_(false) {}

float Meter::addSample(float volts) {
  // Seed the estimate with the first sample so it starts near the real offset
  // instead of drifting up from zero.
  if (!seeded_) {
    dc_ = volts;
    seeded_ = true;
  } else {
    dc_ += (volts - dc_) / kDcFilterWindow;
  }
  return volts - dc_;
}

float Meter::dcLevel() const { return dc_; }
