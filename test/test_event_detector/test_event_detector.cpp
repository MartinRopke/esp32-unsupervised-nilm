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
  for (int i = 0; i < 4; ++i) {
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
  for (int i = 0; i < 4; ++i) {
    result = detector.addSample(highVa - step, timestamp);
    timestamp += kSampleIntervalMicros;
  }

  TEST_ASSERT_TRUE(result.eventDetected);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOff),
                        static_cast<int>(result.event.direction));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, step, result.event.magnitudeVa);
}

// The magnitude is the difference between the two windows' means, not
// between adjacent samples. The crossing sample (140) opens the confirmation
// window but is excluded from it; the candidate window's samples (140, 160,
// 150) average to 150; an adjacent-sample calculation would instead compare
// the last baseline sample (100) to the first candidate sample (140), giving
// 40 -- a result loose enough that a tight tolerance around the true step of
// 50 distinguishes the two.
void test_magnitude_uses_window_means_not_adjacent_samples(void) {
  EventDetector detector(makeConfig());
  const float baselineVa = 100.0f;
  const float candidateSamples[] = {140.0f, 140.0f, 160.0f, 150.0f};

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

// A stable plateau, sustained well past the confirmation window, never
// crosses the threshold relative to its own baseline, so it produces no
// events over a long synthetic run.
void test_stable_plateau_produces_no_repeated_events_over_long_run(void) {
  EventDetector detector(makeConfig());
  const float plateauVa = 100.0f;

  uint32_t timestamp = 0;
  bool anyEventDetected = false;
  for (int i = 0; i < 200; ++i) {
    DetectionResult result = detector.addSample(plateauVa, timestamp);
    timestamp += kSampleIntervalMicros;
    anyEventDetected |= result.eventDetected;
  }

  TEST_ASSERT_FALSE(anyEventDetected);
}

// Two distinct threshold crossings that both land inside the same
// confirmation window are absorbed into one event, not two: the second
// crossing arrives while the detector is already confirming the first, so
// updateConfirming accumulates it into the candidate window instead of
// re-evaluating it against the (stale) baseline.
void test_two_steps_inside_confirmation_window_produce_single_event(void) {
  EventDetector detector(makeConfig());
  const float baselineVa = 100.0f;

  uint32_t timestamp = 0;
  FeedRun baseline = feedPlateau(detector, baselineVa, 3, timestamp);
  timestamp = baseline.timestamp;

  // Sample 1 crosses the threshold and opens the confirmation window.
  int eventCount = 0;
  DetectionResult result = detector.addSample(baselineVa + 50.0f, timestamp);
  timestamp += kSampleIntervalMicros;
  if (result.eventDetected) ++eventCount;

  // Sample 2, while still confirming, is a second, larger jump -- a step of
  // its own if compared against the baseline directly. It must not produce
  // a second event.
  result = detector.addSample(baselineVa + 200.0f, timestamp);
  timestamp += kSampleIntervalMicros;
  if (result.eventDetected) ++eventCount;

  // Sample 3, while still confirming, does not yet fill the candidate
  // window: the crossing sample (1) is excluded from it, so the window only
  // has samples 2 and 3 so far.
  result = detector.addSample(baselineVa + 200.0f, timestamp);
  timestamp += kSampleIntervalMicros;
  if (result.eventDetected) ++eventCount;

  // Sample 4 fills the candidate window and resolves the single merged
  // event.
  result = detector.addSample(baselineVa + 200.0f, timestamp);
  if (result.eventDetected) ++eventCount;

  TEST_ASSERT_EQUAL_INT(1, eventCount);
  TEST_ASSERT_TRUE(result.eventDetected);
}

// A multi-second ramp -- power climbing sample by sample past the threshold,
// as a switching transient settles -- resolves to exactly one event once the
// confirmation window fills, rather than one event per sample that happens
// to clear the threshold against the (stale) baseline.
void test_multi_second_ramp_does_not_fragment_into_several_events(void) {
  EventDetector detector(makeConfig());
  const float baselineVa = 100.0f;
  const float ramp[] = {118.0f, 136.0f, 154.0f, 172.0f, 190.0f, 190.0f};

  uint32_t timestamp = 0;
  FeedRun baseline = feedPlateau(detector, baselineVa, 3, timestamp);
  timestamp = baseline.timestamp;

  int eventCount = 0;
  DetectionResult result{};
  for (float sample : ramp) {
    result = detector.addSample(sample, timestamp);
    timestamp += kSampleIntervalMicros;
    if (result.eventDetected) ++eventCount;
  }

  TEST_ASSERT_EQUAL_INT(1, eventCount);

  // The ramp's final plateau is itself stable: no further events follow.
  FeedRun settled = feedPlateau(detector, 190.0f, 5, timestamp);
  TEST_ASSERT_FALSE(settled.lastResult.eventDetected);
}

// The sample that straddles the switching instant is partial -- part old
// state, part new -- and including it in the candidate window pulls the
// resolved magnitude short of the true step. The next window, sitting on the
// real plateau, then differs from that short baseline by enough to clear the
// threshold again, fragmenting one action into two events. Values are the
// measured series from ISSUE-exclude-transition-sample.md: the sandwich
// maker switching on in event-detection-session-1.csv, t=502..511.
void test_partial_transition_sample_excluded_from_candidate_window(void) {
  EventDetector detector(makeConfig());

  uint32_t timestamp = 0;
  const float baselineSamples[] = {0.20f, 0.21f, 0.21f};
  for (float sample : baselineSamples) {
    detector.addSample(sample, timestamp);
    timestamp += kSampleIntervalMicros;
  }

  const uint32_t crossingTimestamp = timestamp;
  const float transitionSamples[] = {699.41f, 793.41f, 793.61f, 793.11f, 792.99f, 793.26f};
  int eventCount = 0;
  DetectionResult lastEvent{};
  for (float sample : transitionSamples) {
    DetectionResult result = detector.addSample(sample, timestamp);
    timestamp += kSampleIntervalMicros;
    if (result.eventDetected) {
      ++eventCount;
      lastEvent = result;
    }
  }

  TEST_ASSERT_EQUAL_INT(1, eventCount);
  TEST_ASSERT_TRUE(lastEvent.eventDetected);
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 793.0f, lastEvent.event.magnitudeVa);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOn),
                        static_cast<int>(lastEvent.event.direction));
  TEST_ASSERT_EQUAL_UINT32(crossingTimestamp, lastEvent.event.timestampMicros);
}

// Mirror of the case above for switching off, taken from the same file: the
// same sandwich maker switching off a few seconds later,
// event-detection-session-1.csv, t=518..527.
void test_partial_transition_sample_excluded_from_candidate_window_switch_off(void) {
  EventDetector detector(makeConfig());

  uint32_t timestamp = 0;
  const float baselineSamples[] = {793.9458f, 789.8550f, 791.2831f};
  for (float sample : baselineSamples) {
    detector.addSample(sample, timestamp);
    timestamp += kSampleIntervalMicros;
  }

  const uint32_t crossingTimestamp = timestamp;
  const float transitionSamples[] = {748.5990f, 0.8808f, 0.2265f, 0.1976f, 0.1862f, 0.2027f};
  int eventCount = 0;
  DetectionResult lastEvent{};
  for (float sample : transitionSamples) {
    DetectionResult result = detector.addSample(sample, timestamp);
    timestamp += kSampleIntervalMicros;
    if (result.eventDetected) {
      ++eventCount;
      lastEvent = result;
    }
  }

  TEST_ASSERT_EQUAL_INT(1, eventCount);
  TEST_ASSERT_TRUE(lastEvent.eventDetected);
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 791.3f, lastEvent.event.magnitudeVa);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOff),
                        static_cast<int>(lastEvent.event.direction));
  TEST_ASSERT_EQUAL_UINT32(crossingTimestamp, lastEvent.event.timestampMicros);
}

// A step around the fan's size (~45 VA, well under the S/3 > threshold fragmentation
// bound worked out in the issue) must resolve to exactly one event whether
// or not the crossing sample is in the candidate window, so the fix is not
// achieved by making the detector less sensitive. Taken from the same file:
// the fan switching on, event-detection-session-1.csv, t=1648..1657.
void test_fan_sized_step_stays_single_event_regardless_of_transition_sample_handling(void) {
  EventDetector detector(makeConfig());

  uint32_t timestamp = 0;
  const float baselineSamples[] = {0.2005f, 0.2002f, 0.2055f};
  for (float sample : baselineSamples) {
    detector.addSample(sample, timestamp);
    timestamp += kSampleIntervalMicros;
  }

  const float transitionSamples[] = {55.1644f, 57.4154f, 46.7353f, 45.5001f, 45.4983f, 45.5046f};
  int eventCount = 0;
  DetectionResult lastEvent{};
  for (float sample : transitionSamples) {
    DetectionResult result = detector.addSample(sample, timestamp);
    timestamp += kSampleIntervalMicros;
    if (result.eventDetected) {
      ++eventCount;
      lastEvent = result;
    }
  }

  TEST_ASSERT_EQUAL_INT(1, eventCount);
  TEST_ASSERT_TRUE(lastEvent.eventDetected);
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 50.0f, lastEvent.event.magnitudeVa);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOn),
                        static_cast<int>(lastEvent.event.direction));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_step_above_threshold_produces_one_event_with_close_magnitude);
  RUN_TEST(test_noise_below_threshold_produces_no_event);
  RUN_TEST(test_switch_off_detected_with_opposite_direction);
  RUN_TEST(test_magnitude_uses_window_means_not_adjacent_samples);
  RUN_TEST(test_stable_plateau_produces_no_repeated_events_over_long_run);
  RUN_TEST(test_two_steps_inside_confirmation_window_produce_single_event);
  RUN_TEST(test_multi_second_ramp_does_not_fragment_into_several_events);
  RUN_TEST(test_partial_transition_sample_excluded_from_candidate_window);
  RUN_TEST(test_partial_transition_sample_excluded_from_candidate_window_switch_off);
  RUN_TEST(test_fan_sized_step_stays_single_event_regardless_of_transition_sample_handling);
  return UNITY_END();
}
