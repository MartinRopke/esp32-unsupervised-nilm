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

bool EventMerger::withinMergeWindow(uint32_t timestampMicros) const {
  float elapsedSeconds = (timestampMicros - lastFragmentDatedMicros_) / 1e6f;
  return elapsedSeconds <= config_.mergeWindowSeconds;
}

bool EventMerger::pastReleaseWindow(uint32_t nowMicros) const {
  float elapsedSeconds = (nowMicros - lastFragmentArrivalMicros_) / 1e6f;
  return elapsedSeconds > config_.mergeWindowSeconds;
}
