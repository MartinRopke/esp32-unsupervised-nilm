#pragma once

#include <cstdint>

#include "event_detector.h"
#include "meter.h"

// Prints the bench-session CSV row to Serial. Depends on Arduino's Serial, so
// this stays in src/ rather than lib/ -- lib/session_csv/ keeps the actual
// formatting pure and native-testable; this is just the I/O wrapper around it.

// No-op unless result.windowClosed -- mirrors SampleResult's own contract
// that the closed-window fields are only meaningful once a window has
// closed, same as printTeleplotWindow.
void printSessionCsvRow(uint32_t timestampMicros, const SampleResult& result,
                        const DetectionResult& detection);
