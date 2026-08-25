#include "session_csv_output.h"

#include <Arduino.h>

#include "session_csv.h"

void printSessionCsvRow(uint32_t timestampMicros, const SampleResult& result,
                        const DetectionResult& detection) {
  if (!result.windowClosed) return;

  // event_t_s is the detector's own instant for the transition (the confirmation window's
  // first sample), which precedes this row's t_s by up to the confirmation window's latency:
  // collapsing the two would lose which sample actually marked the transition.
  Serial.println(formatSessionCsvRow(timestampMicros, result, detection).c_str());
}
