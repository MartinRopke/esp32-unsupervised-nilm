#include "event_detector.h"

#include <cmath>

EventDetector::EventDetector(const EventDetectorConfig& config)
    : config_(config),
      baseline_(config.confirmationWindowSamples),
      candidate_(config.confirmationWindowSamples) {}

DetectionResult EventDetector::addSample(float apparentPowerVa, uint32_t timestampMicros) {
  return isConfirmingTransition_ ? updateConfirming(apparentPowerVa)
                                 : updateWaiting(apparentPowerVa, timestampMicros);
}

DetectionResult EventDetector::updateWaiting(float apparentPowerVa, uint32_t timestampMicros) {
  bool isCandidateTransition =
      baseline_.isFull() && std::fabs(apparentPowerVa - baseline_.mean()) > config_.thresholdVa;
  if (isCandidateTransition) {
    beginConfirming(timestampMicros);
  } else {
    baseline_.push(apparentPowerVa);
  }
  return {false, {}};
}

void EventDetector::beginConfirming(uint32_t timestampMicros) {
  isConfirmingTransition_ = true;
  candidate_.reset();
  pendingTimestampMicros_ = timestampMicros;
}

DetectionResult EventDetector::updateConfirming(float apparentPowerVa) {
  candidate_.push(apparentPowerVa);
  if (!candidate_.isFull()) {
    return {false, {}};
  }

  float magnitude = candidate_.mean() - baseline_.mean();
  isConfirmingTransition_ = false;
  baseline_ = candidate_;

  if (std::fabs(magnitude) <= config_.thresholdVa) {
    return {false, {}};
  }

  Direction direction = magnitude > 0.0f ? Direction::kOn : Direction::kOff;
  return {true, {pendingTimestampMicros_, std::fabs(magnitude), direction}};
}

EventDetector::MeanWindow::MeanWindow(uint32_t capacity)
    : capacity_(capacity), buffer_(capacity, 0.0f) {}

void EventDetector::MeanWindow::push(float value) {
  if (count_ == capacity_) {
    sum_ -= buffer_[nextIndex_];
  } else {
    ++count_;
  }
  buffer_[nextIndex_] = value;
  sum_ += value;
  nextIndex_ = (nextIndex_ + 1) % capacity_;
}

void EventDetector::MeanWindow::reset() {
  nextIndex_ = 0;
  count_ = 0;
  sum_ = 0.0f;
}

bool EventDetector::MeanWindow::isFull() const { return count_ >= capacity_; }

float EventDetector::MeanWindow::mean() const { return sum_ / static_cast<float>(count_); }
