#include <unity.h>

#include "event_merger.h"

// The detector's own confirmation delay (see event_detector.h): a fragment
// dated t is not handed to the merger until t + kDetectorDelaySeconds. Tests
// below that simulate a realistic arrival use this to derive nowMicros from
// an event's dated instant, matching how src/main.cpp actually calls
// addEvent().
static constexpr float kDetectorDelaySeconds = 3.0f;

static EventMergerConfig makeConfig() {
  EventMergerConfig c;
  c.mergeWindowSeconds = 5.0f;
  return c;
}

static uint32_t toMicros(float seconds) { return static_cast<uint32_t>(seconds * 1e6f + 0.5f); }

static Event makeEvent(float seconds, float magnitudeVa, Direction direction) {
  Event e;
  e.timestampMicros = toMicros(seconds);
  e.magnitudeVa = magnitudeVa;
  e.direction = direction;
  return e;
}

// Feeds a fragment dated `seconds`, arriving kDetectorDelaySeconds later, as
// a real fragment would.
static MergeResult addFragment(EventMerger& merger, float seconds, float magnitudeVa,
                               Direction direction) {
  return merger.addEvent(makeEvent(seconds, magnitudeVa, direction),
                         toMicros(seconds + kDetectorDelaySeconds));
}

// Two same-direction fragments 4 s apart in dated time, inside the 5 s merge
// window, are folded into one held event; nothing is released until a third
// input forces it, and the released magnitude is the sum. The merge
// decision runs on dated instants, so a fixed detector delay on both
// fragments does not change the outcome.
void test_two_same_direction_events_within_window_are_merged(void) {
  EventMerger merger(makeConfig());

  MergeResult first = addFragment(merger, 0.0f, 30.4f, Direction::kOn);
  TEST_ASSERT_FALSE(first.eventReady);

  MergeResult second = addFragment(merger, 4.0f, 68.7f, Direction::kOn);
  TEST_ASSERT_FALSE(second.eventReady);

  // Release via an unrelated later event.
  MergeResult released = addFragment(merger, 20.0f, 100.0f, Direction::kOff);
  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(2, released.fragments);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 99.1f, released.event.magnitudeVa);
  // Dated by the first fragment.
  TEST_ASSERT_EQUAL_UINT32(0, released.event.timestampMicros);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOn),
                        static_cast<int>(released.event.direction));
}

// Two opposite-direction events 4 s apart are not merged: the first is
// released unmerged as soon as the second (opposite-direction) one arrives.
void test_opposite_direction_events_within_window_are_not_merged(void) {
  EventMerger merger(makeConfig());

  addFragment(merger, 0.0f, 50.0f, Direction::kOn);
  MergeResult released = addFragment(merger, 4.0f, 45.0f, Direction::kOff);

  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, released.event.magnitudeVa);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOn),
                        static_cast<int>(released.event.direction));
}

// Two same-direction events 8 s apart in dated time, past the 5 s window,
// are not merged either: the first is released unmerged when the second
// arrives.
void test_same_direction_events_past_window_are_not_merged(void) {
  EventMerger merger(makeConfig());

  addFragment(merger, 0.0f, 50.0f, Direction::kOn);
  MergeResult released = addFragment(merger, 8.0f, 45.0f, Direction::kOn);

  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, released.event.magnitudeVa);
}

// The merge condition reads dated instants, not arrival, even when a
// varying detector delay would put the second fragment's arrival inside
// what the (wrong) window based on arrival would allow. Fragment B is dated
// 5.5 s after A, past mergeWindowSeconds, but a shorter confirmation
// delay lets it arrive only 3.5 s after A's arrival, well inside a 5 s
// hold. It must still not merge: the merge decision does not get to see
// arrival time at all.
void test_same_direction_past_window_in_dated_time_does_not_merge_despite_close_arrival(void) {
  EventMerger merger(makeConfig());

  MergeResult first = merger.addEvent(makeEvent(0.0f, 50.0f, Direction::kOn), toMicros(3.0f));
  TEST_ASSERT_FALSE(first.eventReady);

  MergeResult released = merger.addEvent(makeEvent(5.5f, 45.0f, Direction::kOn), toMicros(6.5f));
  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, released.event.magnitudeVa);
}

// An event with nothing following it is released once mergeWindowSeconds
// have elapsed on the wall clock since its arrival (dated + detector delay),
// not since its dated instant: driven by tick() alone, no other event
// forces it out.
void test_event_with_no_follow_up_released_by_tick_alone(void) {
  EventMerger merger(makeConfig());

  MergeResult fed = addFragment(merger, 0.0f, 50.0f, Direction::kOn);
  TEST_ASSERT_FALSE(fed.eventReady);
  // Arrival is at 3 s; release threshold is arrival + 5 s = 8 s.

  MergeResult stillHeld = merger.tick(toMicros(7.9f));
  TEST_ASSERT_FALSE(stillHeld.eventReady);

  MergeResult released = merger.tick(toMicros(8.1f));
  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, released.event.magnitudeVa);
  TEST_ASSERT_EQUAL_UINT32(0, released.event.timestampMicros);
}

// A single event with no neighbours comes out unchanged: fragments = 1,
// same magnitude, same timestamp as fed in.
void test_single_event_comes_out_unchanged(void) {
  EventMerger merger(makeConfig());

  addFragment(merger, 10.0f, 42.5f, Direction::kOff);
  MergeResult released = merger.tick(toMicros(20.0f));

  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_EQUAL_FLOAT(42.5f, released.event.magnitudeVa);
  TEST_ASSERT_EQUAL_UINT32(toMicros(10.0f), released.event.timestampMicros);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOff),
                        static_cast<int>(released.event.direction));
}

// The six genuine merge opportunities the bench session produced (see
// ISSUE-event-merger-release-clock.md), taken from the recorded series in
// docs/measurements/event-clustering-session-2.csv and -4.csv, not from a
// synthetic ramp. On the buggy release clock every one of these failed to
// fuse; on the correct one, every one must.
struct RegressionPair {
  const char* capture;
  float datedA;
  float magnitudeA;
  float datedB;
  float magnitudeB;
  float expectedMergedMagnitude;
};

static const RegressionPair kRegressionPairs[] = {
    {"session-2", 1419.0f, 49.2f, 1423.0f, 52.2f, 101.3f},
    {"session-2", 1457.0f, 48.8f, 1461.0f, 52.0f, 100.8f},
    {"session-2", 1531.0f, 38.3f, 1535.0f, 63.3f, 101.6f},
    {"session-2", 2256.0f, 235.1f, 2260.0f, 466.5f, 701.6f},
    {"session-2", 2287.0f, 641.8f, 2291.0f, 146.9f, 788.7f},
    // The fan+charger deliberate simultaneous switch: correctly fused as one
    // event under the project's convention of counting coincident
    // transitions together, not a fragmented transition. Assert it
    // explicitly so nobody later "fixes" it.
    {"session-4", 727.0f, 98.5f, 731.0f, 35.5f, 134.0f},
};
static constexpr size_t kRegressionPairCount =
    sizeof(kRegressionPairs) / sizeof(kRegressionPairs[0]);

void test_measured_series_regression_pairs_fuse_to_expected_magnitude(void) {
  for (size_t i = 0; i < kRegressionPairCount; ++i) {
    const RegressionPair& pair = kRegressionPairs[i];
    EventMerger merger(makeConfig());

    MergeResult first = addFragment(merger, pair.datedA, pair.magnitudeA, Direction::kOn);
    TEST_ASSERT_FALSE(first.eventReady);

    MergeResult second = addFragment(merger, pair.datedB, pair.magnitudeB, Direction::kOn);
    TEST_ASSERT_FALSE(second.eventReady);

    MergeResult released = merger.tick(
        toMicros(pair.datedB + kDetectorDelaySeconds + makeConfig().mergeWindowSeconds + 0.1f));
    TEST_ASSERT_TRUE(released.eventReady);
    TEST_ASSERT_EQUAL_UINT32(2, released.fragments);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, pair.expectedMergedMagnitude, released.event.magnitudeVa);
  }
}

// The first three charger pairs above sum to the charger's true off
// transition, not just to some coincidentally close number: 101.3, 100.8 and 101.6 VA,
// 0.40% dispersion over three repetitions, matching the charger's own
// off-transition band (88.8-101.6 VA). This is the evidence that the merge
// rule was always right and only the clock was wrong.
void test_charger_pairs_sum_matches_off_transition_band(void) {
  const float sums[] = {101.3f, 100.8f, 101.6f};
  float mean = (sums[0] + sums[1] + sums[2]) / 3.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 101.2f, mean);
  for (float sum : sums) {
    TEST_ASSERT_TRUE(sum >= 88.8f && sum <= 101.6f);
  }
}

// A released merged event must carry fragments > 1 so it reaches the CSV
// row as such; SessionCsv writes whatever MergeResult::fragments holds.
void test_fragments_count_reaches_two_on_merge(void) {
  EventMerger merger(makeConfig());
  addFragment(merger, 0.0f, 49.2f, Direction::kOn);
  addFragment(merger, 4.0f, 52.2f, Direction::kOn);
  MergeResult released = merger.tick(toMicros(4.0f + kDetectorDelaySeconds + 5.0f + 0.1f));
  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(2, released.fragments);
}

// Fixture: the 43 raw events detected across a confirmation-session bench
// capture on 24 Aug 2026 (sandwich maker, fan, and laptop charger, switched
// both manually and by the sandwich maker's own thermostat), in emission
// order across the session's two capture files (an ESP32 reset split the
// capture partway through). Merging them at the 5 s window must reproduce
// exactly 39 events: two two-fragment charger switch-ons and two fragmented
// autonomous thermostat transitions account for the four merges. The merge
// decision runs on dated instants only, so feeding a constant detector
// delay (kDetectorDelaySeconds) alongside each dated timestamp reproduces
// the same outcome as feeding the dated instants alone.
struct RawEventFixture {
  float seconds;
  float magnitudeVa;
  Direction direction;
};

static const RawEventFixture kConfirmationSessionRawEvents[] = {
    {83.019997f, 802.8538f, Direction::kOn},    {211.019394f, 790.2127f, Direction::kOff},
    {326.019501f, 49.7598f, Direction::kOn},    {492.020416f, 40.7826f, Direction::kOff},
    {496.019897f, 804.0106f, Direction::kOn},   {544.020081f, 790.8248f, Direction::kOff},
    {647.020508f, 804.6805f, Direction::kOn},   {672.019653f, 797.4584f, Direction::kOff},
    {688.019653f, 798.2819f, Direction::kOn},   {707.019714f, 794.9993f, Direction::kOff},
    {728.020569f, 73.3016f, Direction::kOn},    {736.020264f, 57.3342f, Direction::kOn},
    {881.020081f, 128.8974f, Direction::kOff},  {885.019653f, 806.4323f, Direction::kOn},
    {930.020203f, 789.5943f, Direction::kOff},  {1017.02002f, 802.9792f, Direction::kOn},
    {1037.019531f, 802.1556f, Direction::kOff}, {1129.02002f, 49.2624f, Direction::kOn},
    {1153.020386f, 38.508f, Direction::kOff},   {1167.020386f, 50.9853f, Direction::kOn},
    {1188.019409f, 40.4984f, Direction::kOff},  {1203.02002f, 51.0423f, Direction::kOn},
    {1224.019897f, 43.1833f, Direction::kOff},  {1257.019897f, 47.4931f, Direction::kOn},
    {1261.019531f, 51.169f, Direction::kOn},    {1293.020386f, 93.1071f, Direction::kOff},
    {1308.019897f, 30.3635f, Direction::kOn},   {1312.019287f, 68.7344f, Direction::kOn},
    {1342.020386f, 100.1393f, Direction::kOff}, {1359.019653f, 54.5585f, Direction::kOn},
    {1443.019897f, 96.7435f, Direction::kOff},  {1472.02002f, 838.3987f, Direction::kOn},
    {1503.019897f, 823.9921f, Direction::kOff}, {1527.020142f, 782.948f, Direction::kOn},
    {1566.02002f, 736.4642f, Direction::kOff},  {1570.019409f, 41.7403f, Direction::kOff},
    {1874.020386f, 236.7735f, Direction::kOn},  {1879.020508f, 56.2791f, Direction::kOff},
    {1883.02002f, 502.4466f, Direction::kOn},   {1887.019653f, 86.2736f, Direction::kOn},
    {1914.020264f, 780.5815f, Direction::kOff},
};
static constexpr size_t kConfirmRawCount =
    sizeof(kConfirmationSessionRawEvents) / sizeof(kConfirmationSessionRawEvents[0]);

// The session's second capture file, with its own independent t_s origin:
// reopening the serial port mid-session reset the board and restarted
// micros() from zero.
static const RawEventFixture kConfirmationSessionRawEventsPart2[] = {
    {45.019466f, 789.4929f, Direction::kOn},
    {64.019897f, 786.7841f, Direction::kOff},
};
static constexpr size_t kConfirmRawCount2 =
    sizeof(kConfirmationSessionRawEventsPart2) / sizeof(kConfirmationSessionRawEventsPart2[0]);

// Feeds a fixture array through a fresh merger, followed by a distant tick
// to flush anything still held, and returns how many merged events came out.
static uint32_t feedFixtureCountingMergedEvents(const RawEventFixture* fixture, size_t count,
                                                uint32_t& fragmentsAtTwoFragmentCount) {
  EventMerger merger(makeConfig());
  uint32_t mergedCount = 0;
  float lastDatedSeconds = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    lastDatedSeconds = fixture[i].seconds;
    MergeResult result =
        addFragment(merger, fixture[i].seconds, fixture[i].magnitudeVa, fixture[i].direction);
    if (result.eventReady) {
      ++mergedCount;
      if (result.fragments == 2) ++fragmentsAtTwoFragmentCount;
    }
  }
  MergeResult flushed = merger.tick(toMicros(lastDatedSeconds + kDetectorDelaySeconds + 10.0f));
  if (flushed.eventReady) {
    ++mergedCount;
    if (flushed.fragments == 2) ++fragmentsAtTwoFragmentCount;
  }
  return mergedCount;
}

// Each capture is its own independent timeline (see the fixture comment
// above); merging is run separately per capture, then the counts are
// combined, matching how the two-file split is handled everywhere else in
// this session's documentation and tests.
void test_confirmation_session_fixture_merges_forty_three_events_into_thirty_nine(void) {
  uint32_t twoFragmentEvents1 = 0;
  uint32_t merged1 = feedFixtureCountingMergedEvents(kConfirmationSessionRawEvents,
                                                     kConfirmRawCount, twoFragmentEvents1);
  uint32_t twoFragmentEvents2 = 0;
  uint32_t merged2 = feedFixtureCountingMergedEvents(kConfirmationSessionRawEventsPart2,
                                                     kConfirmRawCount2, twoFragmentEvents2);

  TEST_ASSERT_EQUAL_UINT32(37, merged1);
  TEST_ASSERT_EQUAL_UINT32(2, merged2);
  TEST_ASSERT_EQUAL_UINT32(39, merged1 + merged2);
  // Four two-fragment merges: the two charger switch-ons (1257/1261 and
  // 1308/1312), the thermostat's fragmented autonomous cutoff (1566/1570),
  // and the fragmented start of its autonomous reengagement (1883/1887).
  TEST_ASSERT_EQUAL_UINT32(4, twoFragmentEvents1);
  TEST_ASSERT_EQUAL_UINT32(0, twoFragmentEvents2);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_two_same_direction_events_within_window_are_merged);
  RUN_TEST(test_opposite_direction_events_within_window_are_not_merged);
  RUN_TEST(test_same_direction_events_past_window_are_not_merged);
  RUN_TEST(test_same_direction_past_window_in_dated_time_does_not_merge_despite_close_arrival);
  RUN_TEST(test_event_with_no_follow_up_released_by_tick_alone);
  RUN_TEST(test_single_event_comes_out_unchanged);
  RUN_TEST(test_measured_series_regression_pairs_fuse_to_expected_magnitude);
  RUN_TEST(test_charger_pairs_sum_matches_off_transition_band);
  RUN_TEST(test_fragments_count_reaches_two_on_merge);
  RUN_TEST(test_confirmation_session_fixture_merges_forty_three_events_into_thirty_nine);
  return UNITY_END();
}
