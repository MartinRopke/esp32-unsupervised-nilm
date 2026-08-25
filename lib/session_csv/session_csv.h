#pragma once

#include <cstdint>
#include <string>

#include "event_detector.h"
#include "meter.h"

// Pure formatter for the bench-session CSV row (t_s,vrms,irms,power,event,
// event_t_s,delta_va,direction). No Arduino / Wire / Serial dependencies, so
// it builds and is unit tested in the `native` environment, same as Meter
// and EventDetector.

// Formats one CSV row for a closed Meter window, without a trailing line
// terminator (the caller prints it). event_t_s, delta_va, and direction are
// left blank -- not zero -- when detection.eventDetected is false, so a row
// with no event is unambiguous from one with a zero-magnitude event.
std::string formatSessionCsvRow(uint32_t timestampMicros, const SampleResult& result,
                                const DetectionResult& detection);
