#pragma once

#include <cstdint>

#include "event_detector.h"

// Pure event-merging module for the reading system. No Arduino / Wire /
// Serial dependencies, so it builds and is unit tested in the `native`
// environment. A transition that spans several sampling intervals reaches
// the detector as 2-4 fragments instead of one; fed those fragments (or, on
// a window that carries none, a tick marking time passing), this module
// fuses consecutive same-direction fragments into a single event and
// releases it once no further fragment arrives inside the merge window.

// Single install configuration block, injected rather than hardcoded.
struct EventMergerConfig {
  float mergeWindowSeconds;  // MERGE_WINDOW_SECONDS  [s]  e.g. 5.0
};

// Result of feeding one input to the EventMerger. `event` and `fragments`
// are only meaningful when `eventReady` is true. `event` is the sum of one
// or more same-direction fragments, timestamped at the first fragment;
// `fragments` is how many were summed (1 if none merged).
struct MergeResult {
  bool eventReady;
  Event event;
  uint32_t fragments;
};

class EventMerger {
 public:
  explicit EventMerger(const EventMergerConfig& config);

  // Feeds one detected event. If an event is already held and this one
  // shares its direction and arrives within mergeWindowSeconds of the held
  // event's last fragment, it is folded in (magnitude summed, fragment
  // count incremented) and nothing is released yet. Otherwise the held
  // event, if any, is released, and this one becomes the newly held event.
  MergeResult addEvent(const Event& event);

  // Advances the merger's clock without a new event. Releases the held
  // event once mergeWindowSeconds have elapsed since its last fragment.
  // Call once per closed sampling window that carries no detected event, so
  // a held event is not stuck waiting for a fragment that never arrives.
  MergeResult tick(uint32_t timestampMicros);

 private:
  // The event and fragment count held while a same-direction follow-up
  // fragment might still arrive.
  struct HeldEvent {
    Event event;
    uint32_t fragments;
  };

  MergeResult release();
  bool withinWindow(uint32_t timestampMicros) const;

  EventMergerConfig config_;
  bool hasPending_ = false;
  HeldEvent pending_{};
  uint32_t lastFragmentMicros_ = 0;
};
