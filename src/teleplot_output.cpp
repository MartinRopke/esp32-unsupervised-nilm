#include "teleplot_output.h"

#include <Arduino.h>

void printTeleplotSample(float dcOffset, float acVolts) {
  Serial.print(">dc:");
  Serial.println(dcOffset, 4);
  Serial.print(">ac:");
  Serial.println(acVolts, 4);
}

void printTeleplotWindow(const SampleResult& result) {
  if (!result.windowClosed) return;

  Serial.print(">vrms:");
  Serial.println(result.vRms, 6);
  Serial.print(">irms:");
  Serial.println(result.iRms, 4);
  Serial.print(">power:");
  Serial.println(result.apparentPower, 4);
}

void printTeleplotEvent(const DetectionResult& detection) {
  if (!detection.eventDetected) return;

  Serial.print(">t_s:");
  Serial.println(detection.event.timestampMicros / 1e6f, 6);
  Serial.print(">delta_va:");
  Serial.println(detection.event.magnitudeVa, 4);

  // Cast to int so Teleplot can plot the event, since it can't plot graph with text token.
  Serial.print(">direction:");
  Serial.println(static_cast<int>(detection.event.direction));
}
