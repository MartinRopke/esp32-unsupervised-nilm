#include <unity.h>

#include <cmath>

#include "event_detector.h"

// A representative install config: 30 VA threshold, 3-sample confirmation
// window, matching the values the issue settles on for the real firmware.
static EventDetectorConfig makeConfig() {
  EventDetectorConfig c;
  c.thresholdVa = 30.0f;
  c.confirmationWindowSamples = 3;
  return c;
}

static constexpr uint32_t kSampleIntervalMicros = 1000000;  // 1 Hz, per closed Meter window

// Feeds `count` samples of `valueVa`, one per second starting at `timestamp`,
// and returns the last DetectionResult along with the advanced timestamp.
struct FeedRun {
  DetectionResult lastResult;
  uint32_t timestamp;
};

static FeedRun feedPlateau(EventDetector& detector, float valueVa, uint32_t count,
                           uint32_t timestamp) {
  DetectionResult result{};
  for (uint32_t i = 0; i < count; ++i) {
    result = detector.addSample(valueVa, timestamp);
    timestamp += kSampleIntervalMicros;
  }
  return {result, timestamp};
}

// A step from one stable plateau to another, both sustained past the
// confirmation window, produces exactly one event once the new plateau's
// window fills, with magnitude close to the step size.
void test_step_above_threshold_produces_one_event_with_close_magnitude(void) {
  EventDetector detector(makeConfig());
  const float baselineVa = 100.0f;
  const float step = 50.0f;

  uint32_t timestamp = 0;
  // Fill the baseline window.
  FeedRun baseline = feedPlateau(detector, baselineVa, 3, timestamp);
  timestamp = baseline.timestamp;
  TEST_ASSERT_FALSE(baseline.lastResult.eventDetected);

  // Feed the new plateau; only the sample that fills the candidate window
  // should report an event.
  int eventCount = 0;
  DetectionResult result{};
  for (int i = 0; i < 3; ++i) {
    result = detector.addSample(baselineVa + step, timestamp);
    timestamp += kSampleIntervalMicros;
    if (result.eventDetected) ++eventCount;
  }

  TEST_ASSERT_EQUAL_INT(1, eventCount);
  TEST_ASSERT_TRUE(result.eventDetected);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, step, result.event.magnitudeVa);
}

// Small fluctuations that never exceed the threshold produce no event, even
// well after the baseline window has filled.
void test_noise_below_threshold_produces_no_event(void) {
  EventDetector detector(makeConfig());
  const float baselineVa = 100.0f;
  const float noise[] = {5.0f, -8.0f, 3.0f, -4.0f, 9.0f, -6.0f, 7.0f, -2.0f};

  uint32_t timestamp = 0;
  bool anyEventDetected = false;
  for (float delta : noise) {
    DetectionResult result = detector.addSample(baselineVa + delta, timestamp);
    timestamp += kSampleIntervalMicros;
    anyEventDetected |= result.eventDetected;
  }

  TEST_ASSERT_FALSE(anyEventDetected);
}

// A downward step (an appliance switching off) is detected as kOff, the
// opposite direction from an upward step.
void test_switch_off_detected_with_opposite_direction(void) {
  EventDetector detector(makeConfig());
  const float highVa = 150.0f;
  const float step = 50.0f;

  uint32_t timestamp = 0;
  FeedRun baseline = feedPlateau(detector, highVa, 3, timestamp);
  timestamp = baseline.timestamp;

  DetectionResult result{};
  for (int i = 0; i < 3; ++i) {
    result = detector.addSample(highVa - step, timestamp);
    timestamp += kSampleIntervalMicros;
  }

  TEST_ASSERT_TRUE(result.eventDetected);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOff),
                        static_cast<int>(result.event.direction));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, step, result.event.magnitudeVa);
}

// The magnitude is the difference between the two windows' means, not
// between adjacent samples. The candidate window's samples (140, 160, 150)
// average to 150; an adjacent-sample calculation would instead compare the
// last baseline sample (100) to the first candidate sample (140), giving 40
// -- a result loose enough that a tight tolerance around the true step of 50
// distinguishes the two.
void test_magnitude_uses_window_means_not_adjacent_samples(void) {
  EventDetector detector(makeConfig());
  const float baselineVa = 100.0f;
  const float candidateSamples[] = {140.0f, 160.0f, 150.0f};

  uint32_t timestamp = 0;
  FeedRun baseline = feedPlateau(detector, baselineVa, 3, timestamp);
  timestamp = baseline.timestamp;

  DetectionResult result{};
  for (float sample : candidateSamples) {
    result = detector.addSample(sample, timestamp);
    timestamp += kSampleIntervalMicros;
  }

  TEST_ASSERT_TRUE(result.eventDetected);
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 50.0f, result.event.magnitudeVa);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_step_above_threshold_produces_one_event_with_close_magnitude);
  RUN_TEST(test_noise_below_threshold_produces_no_event);
  RUN_TEST(test_switch_off_detected_with_opposite_direction);
  RUN_TEST(test_magnitude_uses_window_means_not_adjacent_samples);
  return UNITY_END();
}
