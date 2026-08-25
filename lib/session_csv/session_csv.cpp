#include "session_csv.h"

#include <cstdio>

namespace {

std::string formatFixed(float value, int decimals) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
  return buffer;
}

}  // namespace

std::string formatSessionCsvRow(uint32_t timestampMicros, const SampleResult& result,
                                const MergeResult& merge, int32_t clusterId) {
  const bool hasEvent = merge.eventReady;
  const std::string eventTimestamp =
      hasEvent ? formatFixed(merge.event.timestampMicros / 1e6f, 6) : "";
  const std::string magnitudeVa = hasEvent ? formatFixed(merge.event.magnitudeVa, 4) : "";
  const std::string direction =
      hasEvent ? std::to_string(static_cast<int>(merge.event.direction)) : "";
  const std::string cluster = hasEvent ? std::to_string(clusterId) : "";
  const std::string fragments = hasEvent ? std::to_string(merge.fragments) : "";

  return formatFixed(timestampMicros / 1e6f, 6) + "," + formatFixed(result.vRms, 6) + "," +
         formatFixed(result.iRms, 4) + "," + formatFixed(result.apparentPower, 4) + "," +
         (hasEvent ? "1" : "0") + "," + eventTimestamp + "," + magnitudeVa + "," + direction + "," +
         cluster + "," + fragments;
}
