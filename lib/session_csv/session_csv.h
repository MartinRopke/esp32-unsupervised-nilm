#pragma once

#include <cstdint>
#include <string>

#include "event_merger.h"
#include "meter.h"

// Pure formatter for the bench-session CSV row (t_s,vrms,irms,power,event,
// event_t_s,delta_va,direction,cluster,fragments). No Arduino / Wire /
// Serial dependencies, so it builds and is unit tested in the `native`
// environment, same as Meter, EventDetector, EventMerger and
// EventClusterer.

// Formats one CSV row for a closed Meter window, without a trailing line
// terminator (the caller prints it). event_t_s, delta_va, direction,
// cluster, and fragments are left blank, not zero, when merge.eventReady is
// false, so a row with no event is unambiguous from one with a
// zero-magnitude event. The event fields report the merged event (magnitude
// summed, instant of the first fragment), not the raw per-sample detection:
// fragmented transitions are fused upstream by EventMerger before reaching
// this row. clusterId is only meaningful, and only read, when
// merge.eventReady is true.
std::string formatSessionCsvRow(uint32_t timestampMicros, const SampleResult& result,
                                const MergeResult& merge, int32_t clusterId);
