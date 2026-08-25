#include <unity.h>

#include "session_csv.h"

// A closed window with no event leaves event_t_s, delta_va, and direction
// blank -- not zero -- so downstream parsing can tell "no event" apart from
// a zero-magnitude one.
void test_no_event_row_leaves_event_fields_blank(void) {
  SampleResult result{};
  result.vRms = 0.001234f;
  result.iRms = 0.5678f;
  result.apparentPower = 123.4567f;

  DetectionResult detection{};
  detection.eventDetected = false;

  std::string row = formatSessionCsvRow(3020413, result, detection);

  TEST_ASSERT_EQUAL_STRING("3.020413,0.001234,0.5678,123.4567,0,,,", row.c_str());
}

// A closed window that also carries a detected switch-on event fills
// event_t_s, delta_va, and direction, with direction encoded as the enum's
// underlying int (1 for kOn) -- the bench-capture scripts already parse this
// numeric form, so it can't become a text token.
void test_switch_on_event_row_fills_event_fields(void) {
  SampleResult result{};
  result.vRms = 0.001234f;
  result.iRms = 0.5678f;
  result.apparentPower = 123.4567f;

  DetectionResult detection{};
  detection.eventDetected = true;
  detection.event.timestampMicros = 3018000;
  detection.event.magnitudeVa = 46.5f;
  detection.event.direction = Direction::kOn;

  std::string row = formatSessionCsvRow(3020413, result, detection);

  TEST_ASSERT_EQUAL_STRING("3.020413,0.001234,0.5678,123.4567,1,3.018000,46.5000,1", row.c_str());
}

// A switch-off event encodes direction as -1, the enum's kOff value.
void test_switch_off_event_row_encodes_negative_direction(void) {
  SampleResult result{};
  DetectionResult detection{};
  detection.eventDetected = true;
  detection.event.timestampMicros = 5000000;
  detection.event.magnitudeVa = 50.0f;
  detection.event.direction = Direction::kOff;

  std::string row = formatSessionCsvRow(5000000, result, detection);

  TEST_ASSERT_EQUAL_STRING("5.000000,0.000000,0.0000,0.0000,1,5.000000,50.0000,-1", row.c_str());
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_no_event_row_leaves_event_fields_blank);
  RUN_TEST(test_switch_on_event_row_fills_event_fields);
  RUN_TEST(test_switch_off_event_row_encodes_negative_direction);
  return UNITY_END();
}
