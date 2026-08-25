#pragma once

#include <cstdint>
#include <vector>

// Pure event-detection module for the reading system. No Arduino / Wire /
// Serial dependencies, so it builds and is unit tested in the `native`
// environment. Fed one apparent-power sample per closed Meter window, it
// detects appliance on/off events as steps in that series.

// Single install configuration block. Values are specific to the
// installation and are injected into the EventDetector (never hardcoded in
// the detection logic).
struct EventDetectorConfig {
  float thresholdVa;                   // EVENT_THRESHOLD_VA        e.g. 30
  uint32_t confirmationWindowSamples;  // CONFIRMATION_WINDOW_SAMPLES  e.g. 3
};

enum class Direction { kOn = 1, kOff = -1 };

struct Event {
  uint32_t timestampMicros;
  float magnitudeVa;
  Direction direction;
};

// Result of feeding one sample to the EventDetector. `event` is only
// meaningful when `eventDetected` is true.
struct DetectionResult {
  bool eventDetected;
  Event event;
};

class EventDetector {
 public:
  explicit EventDetector(const EventDetectorConfig& config);

  // Feeds one apparent-power sample (VA) and its timestamp (micros()).
  DetectionResult addSample(float apparentPowerVa, uint32_t timestampMicros);

 private:
  // Not confirming a transition: feeds baseline_ directly, watching for a
  // sample that crosses the threshold.
  DetectionResult updateWaiting(float apparentPowerVa, uint32_t timestampMicros);

  // Confirming a transition: accumulates candidate_ until it fills, then
  // resolves it (candidate_ always becomes the new baseline_; an event is
  // only reported if the resolved magnitude clears the threshold).
  DetectionResult updateConfirming(float apparentPowerVa);

  // The transition from waiting to confirming: starts confirming, dating the
  // pending event to the sample that crossed the threshold. That sample
  // itself straddles the transition -- part old state, part new -- so it is
  // not pushed into candidate_; the candidate window starts filling from the
  // following sample.
  void beginConfirming(uint32_t timestampMicros);

  // Fixed-capacity ring buffer over the last `capacity` apparent-power
  // samples, exposing their mean. Owns only the windowing/averaging policy,
  // independently of when a window is considered a baseline or a candidate.
  class MeanWindow {
   public:
    explicit MeanWindow(uint32_t capacity);

    // Pushes one sample, overwriting the oldest once at capacity.
    void push(float value);

    // Discards all samples; the window must fill again before mean() is
    // meaningful.
    void reset();

    bool isFull() const;

    // Only meaningful once isFull() is true.
    float mean() const;

   private:
    // Not const: EventDetector reassigns whole MeanWindow objects (candidate
    // becomes the new baseline on a confirmed event), which needs the
    // implicit copy-assignment operator.
    uint32_t capacity_;
    std::vector<float> buffer_;
    uint32_t nextIndex_ = 0;
    uint32_t count_ = 0;
    float sum_ = 0.0f;
  };

  EventDetectorConfig config_;
  MeanWindow baseline_;
  MeanWindow candidate_;
  bool isConfirmingTransition_ = false;
  uint32_t pendingTimestampMicros_ = 0;
};
