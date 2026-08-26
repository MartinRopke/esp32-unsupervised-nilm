#pragma once

#include "event_detector.h"
#include "meter.h"

// Serial print helpers for the live-visualization Teleplot >key:value lines.
// Depends on Arduino's Serial, so this stays in src/ rather than lib/:
// there's no logic here worth protecting with a native test, just direct
// prints in the order the loop() call sites need them.

void printTeleplotSample(float dcOffset, float acVolts);

// No-op unless result.windowClosed: mirrors SampleResult's own contract that
// vRms/iRms/apparentPower are only meaningful once a window has closed.
void printTeleplotWindow(const SampleResult& result);

// No-op unless detection.eventDetected: mirrors DetectionResult's own
// contract that event is only meaningful once eventDetected is true.
void printTeleplotEvent(const DetectionResult& detection);
