#include "session_csv_output.h"

#include <Arduino.h>

#include "session_csv.h"

void printSessionCsvRow(uint32_t timestampMicros, const SampleResult& result,
                        const MergeResult& merge, int32_t clusterId) {
  if (!result.windowClosed) return;

  // event_t_s is the first fragment's own instant (detector confirmation-window latency, plus
  // however long the merger held it open), which can precede this row's t_s well beyond a
  // single window: collapsing the two would lose which sample actually marked the transition.
  Serial.println(formatSessionCsvRow(timestampMicros, result, merge, clusterId).c_str());
}
