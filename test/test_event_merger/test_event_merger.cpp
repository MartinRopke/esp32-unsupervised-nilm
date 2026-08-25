#include <unity.h>

#include "event_merger.h"

static EventMergerConfig makeConfig() {
  EventMergerConfig c;
  c.mergeWindowSeconds = 5.0f;
  return c;
}

static Event makeEvent(float seconds, float magnitudeVa, Direction direction) {
  Event e;
  e.timestampMicros = static_cast<uint32_t>(seconds * 1e6f + 0.5f);
  e.magnitudeVa = magnitudeVa;
  e.direction = direction;
  return e;
}

// Two same-direction fragments 4 s apart, inside the 5 s merge window, are
// folded into one held event; nothing is released until a third input
// forces it, and the released magnitude is the sum.
void test_two_same_direction_events_within_window_are_merged(void) {
  EventMerger merger(makeConfig());

  MergeResult first = merger.addEvent(makeEvent(0.0f, 30.4f, Direction::kOn));
  TEST_ASSERT_FALSE(first.eventReady);

  MergeResult second = merger.addEvent(makeEvent(4.0f, 68.7f, Direction::kOn));
  TEST_ASSERT_FALSE(second.eventReady);

  // Release via an unrelated later event.
  MergeResult released = merger.addEvent(makeEvent(20.0f, 100.0f, Direction::kOff));
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

  merger.addEvent(makeEvent(0.0f, 50.0f, Direction::kOn));
  MergeResult released = merger.addEvent(makeEvent(4.0f, 45.0f, Direction::kOff));

  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, released.event.magnitudeVa);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOn),
                        static_cast<int>(released.event.direction));
}

// Two same-direction events 8 s apart, past the 5 s window, are not merged
// either: the first is released unmerged when the second arrives.
void test_same_direction_events_past_window_are_not_merged(void) {
  EventMerger merger(makeConfig());

  merger.addEvent(makeEvent(0.0f, 50.0f, Direction::kOn));
  MergeResult released = merger.addEvent(makeEvent(8.0f, 45.0f, Direction::kOn));

  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, released.event.magnitudeVa);
}

// An event with nothing following it is released once the window elapses,
// driven by time alone: ticks, not another event, advance the clock.
void test_event_with_no_follow_up_released_by_tick_alone(void) {
  EventMerger merger(makeConfig());

  MergeResult fed = merger.addEvent(makeEvent(0.0f, 50.0f, Direction::kOn));
  TEST_ASSERT_FALSE(fed.eventReady);

  MergeResult stillHeld = merger.tick(static_cast<uint32_t>(4.9f * 1e6f));
  TEST_ASSERT_FALSE(stillHeld.eventReady);

  MergeResult released = merger.tick(static_cast<uint32_t>(5.1f * 1e6f));
  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, released.event.magnitudeVa);
  TEST_ASSERT_EQUAL_UINT32(0, released.event.timestampMicros);
}

// A single event with no neighbours comes out unchanged: fragments = 1,
// same magnitude, same timestamp as fed in.
void test_single_event_comes_out_unchanged(void) {
  EventMerger merger(makeConfig());

  merger.addEvent(makeEvent(10.0f, 42.5f, Direction::kOff));
  MergeResult released = merger.tick(static_cast<uint32_t>(20.0f * 1e6f));

  TEST_ASSERT_TRUE(released.eventReady);
  TEST_ASSERT_EQUAL_UINT32(1, released.fragments);
  TEST_ASSERT_EQUAL_FLOAT(42.5f, released.event.magnitudeVa);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(10.0f * 1e6f), released.event.timestampMicros);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Direction::kOff),
                        static_cast<int>(released.event.direction));
}

// Fixture: the 43 raw events detected across a confirmation-session bench
// capture on 24 Aug 2026 (sandwich maker, fan, and laptop charger, switched
// both manually and by the sandwich maker's own thermostat), in emission
// order across the session's two capture files (an ESP32 reset split the
// capture partway through). Merging them at the 5 s window must reproduce
// exactly 39 events: two two-fragment charger switch-ons and two fragmented
// autonomous thermostat transitions account for the four merges.
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
  uint32_t lastTimestamp = 0;
  for (size_t i = 0; i < count; ++i) {
    Event event = makeEvent(fixture[i].seconds, fixture[i].magnitudeVa, fixture[i].direction);
    lastTimestamp = event.timestampMicros;
    MergeResult result = merger.addEvent(event);
    if (result.eventReady) {
      ++mergedCount;
      if (result.fragments == 2) ++fragmentsAtTwoFragmentCount;
    }
  }
  MergeResult flushed = merger.tick(lastTimestamp + static_cast<uint32_t>(10.0f * 1e6f));
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
  RUN_TEST(test_event_with_no_follow_up_released_by_tick_alone);
  RUN_TEST(test_single_event_comes_out_unchanged);
  RUN_TEST(test_confirmation_session_fixture_merges_forty_three_events_into_thirty_nine);
  return UNITY_END();
}
