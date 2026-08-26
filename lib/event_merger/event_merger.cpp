#include "event_merger.h"

EventMerger::EventMerger(const EventMergerConfig& config) : config_(config) {}

MergeResult EventMerger::addEvent(const Event& event, uint32_t nowMicros) {
  if (hasPending_ && event.direction == pending_.event.direction &&
      withinMergeWindow(event.timestampMicros)) {
    pending_.event.magnitudeVa += event.magnitudeVa;
    ++pending_.fragments;
    lastFragmentDatedMicros_ = event.timestampMicros;
    lastFragmentArrivalMicros_ = nowMicros;
    return {false, {}, 0};
  }

  MergeResult released = release();
  pending_ = {event, 1};
  hasPending_ = true;
  lastFragmentDatedMicros_ = event.timestampMicros;
  lastFragmentArrivalMicros_ = nowMicros;
  return released;
}

MergeResult EventMerger::tick(uint32_t nowMicros) {
  if (hasPending_ && pastReleaseWindow(nowMicros)) {
    return release();
  }
  return {false, {}, 0};
}

MergeResult EventMerger::release() {
  if (!hasPending_) return {false, {}, 0};
  hasPending_ = false;
  return {true, pending_.event, pending_.fragments};
}

float EventMerger::mergeWindowSeconds() const {
  return config_.mergeWindowSamples * config_.sampleIntervalSeconds;
}

bool EventMerger::withinMergeWindow(uint32_t timestampMicros) const {
  // Half a sampling interval of tolerance on the dated clock only: it
  // absorbs the RMS-window-close jitter documented on EventMergerConfig
  // without also admitting a gap a full interval past the nominal window.
  float jitterToleranceSeconds = 0.5f * config_.sampleIntervalSeconds;
  float elapsedSeconds = (timestampMicros - lastFragmentDatedMicros_) / 1e6f;
  return elapsedSeconds <= mergeWindowSeconds() + jitterToleranceSeconds;
}

bool EventMerger::pastReleaseWindow(uint32_t nowMicros) const {
  float elapsedSeconds = (nowMicros - lastFragmentArrivalMicros_) / 1e6f;
  return elapsedSeconds > mergeWindowSeconds();
}
