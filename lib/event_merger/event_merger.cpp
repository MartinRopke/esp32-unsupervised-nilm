#include "event_merger.h"

EventMerger::EventMerger(const EventMergerConfig& config) : config_(config) {}

MergeResult EventMerger::addEvent(const Event& event) {
  if (hasPending_ && event.direction == pending_.event.direction &&
      withinWindow(event.timestampMicros)) {
    pending_.event.magnitudeVa += event.magnitudeVa;
    ++pending_.fragments;
    lastFragmentMicros_ = event.timestampMicros;
    return {false, {}, 0};
  }

  MergeResult released = release();
  pending_ = {event, 1};
  hasPending_ = true;
  lastFragmentMicros_ = event.timestampMicros;
  return released;
}

MergeResult EventMerger::tick(uint32_t timestampMicros) {
  if (hasPending_ && !withinWindow(timestampMicros)) {
    return release();
  }
  return {false, {}, 0};
}

MergeResult EventMerger::release() {
  if (!hasPending_) return {false, {}, 0};
  hasPending_ = false;
  return {true, pending_.event, pending_.fragments};
}

bool EventMerger::withinWindow(uint32_t timestampMicros) const {
  float elapsedSeconds = (timestampMicros - lastFragmentMicros_) / 1e6f;
  return elapsedSeconds <= config_.mergeWindowSeconds;
}
