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
                                const DetectionResult& detection) {
  const bool hasEvent = detection.eventDetected;
  const std::string eventTimestamp =
      hasEvent ? formatFixed(detection.event.timestampMicros / 1e6f, 6) : "";
  const std::string magnitudeVa = hasEvent ? formatFixed(detection.event.magnitudeVa, 4) : "";
  const std::string direction =
      hasEvent ? std::to_string(static_cast<int>(detection.event.direction)) : "";

  return formatFixed(timestampMicros / 1e6f, 6) + "," + formatFixed(result.vRms, 6) + "," +
         formatFixed(result.iRms, 4) + "," + formatFixed(result.apparentPower, 4) + "," +
         (hasEvent ? "1" : "0") + "," + eventTimestamp + "," + magnitudeVa + "," + direction;
}
