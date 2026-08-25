#include <unity.h>

#include "session_csv.h"

// A closed window with no event leaves event_t_s, delta_va, direction,
// cluster, and fragments blank, not zero, so downstream parsing can tell
// "no event" apart from a zero-magnitude one.
void test_no_event_row_leaves_event_fields_blank(void) {
  SampleResult result{};
  result.vRms = 0.001234f;
  result.iRms = 0.5678f;
  result.apparentPower = 123.4567f;

  MergeResult merge{};
  merge.eventReady = false;

  std::string row = formatSessionCsvRow(3020413, result, merge, /*clusterId=*/0);

  TEST_ASSERT_EQUAL_STRING("3.020413,0.001234,0.5678,123.4567,0,,,,,", row.c_str());
}

// A closed window that carries a released (possibly merged) switch-on event
// fills event_t_s, delta_va, direction, cluster, and fragments, with
// direction encoded as the enum's underlying int (1 for kOn): the
// bench-capture scripts already parse this numeric form, so it can't become
// a text token.
void test_switch_on_event_row_fills_event_fields(void) {
  SampleResult result{};
  result.vRms = 0.001234f;
  result.iRms = 0.5678f;
  result.apparentPower = 123.4567f;

  MergeResult merge{};
  merge.eventReady = true;
  merge.event.timestampMicros = 3018000;
  merge.event.magnitudeVa = 46.5f;
  merge.event.direction = Direction::kOn;
  merge.fragments = 1;

  std::string row = formatSessionCsvRow(3020413, result, merge, /*clusterId=*/2);

  TEST_ASSERT_EQUAL_STRING("3.020413,0.001234,0.5678,123.4567,1,3.018000,46.5000,1,2,1",
                           row.c_str());
}

// A switch-off event encodes direction as -1, the enum's kOff value.
void test_switch_off_event_row_encodes_negative_direction(void) {
  SampleResult result{};
  MergeResult merge{};
  merge.eventReady = true;
  merge.event.timestampMicros = 5000000;
  merge.event.magnitudeVa = 50.0f;
  merge.event.direction = Direction::kOff;
  merge.fragments = 1;

  std::string row = formatSessionCsvRow(5000000, result, merge, /*clusterId=*/1);

  TEST_ASSERT_EQUAL_STRING("5.000000,0.000000,0.0000,0.0000,1,5.000000,50.0000,-1,1,1",
                           row.c_str());
}

// An outlier event (clusterId -1) is reported as -1 in the cluster column,
// not blank: only the absence of an event is blank.
void test_outlier_event_row_reports_negative_one_cluster(void) {
  SampleResult result{};
  MergeResult merge{};
  merge.eventReady = true;
  merge.event.timestampMicros = 1000000;
  merge.event.magnitudeVa = 73.3f;
  merge.event.direction = Direction::kOn;
  merge.fragments = 1;

  std::string row = formatSessionCsvRow(1000000, result, merge, /*clusterId=*/-1);

  TEST_ASSERT_EQUAL_STRING("1.000000,0.000000,0.0000,0.0000,1,1.000000,73.3000,1,-1,1",
                           row.c_str());
}

// A merged event carrying more than one fragment reports the summed
// magnitude and the fragment count, not 1.
void test_merged_event_row_reports_fragment_count_above_one(void) {
  SampleResult result{};
  MergeResult merge{};
  merge.eventReady = true;
  merge.event.timestampMicros = 2000000;
  merge.event.magnitudeVa = 99.0979f;
  merge.event.direction = Direction::kOn;
  merge.fragments = 2;

  std::string row = formatSessionCsvRow(2005000, result, merge, /*clusterId=*/3);

  TEST_ASSERT_EQUAL_STRING("2.005000,0.000000,0.0000,0.0000,1,2.000000,99.0979,1,3,2", row.c_str());
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_no_event_row_leaves_event_fields_blank);
  RUN_TEST(test_switch_on_event_row_fills_event_fields);
  RUN_TEST(test_switch_off_event_row_encodes_negative_direction);
  RUN_TEST(test_outlier_event_row_reports_negative_one_cluster);
  RUN_TEST(test_merged_event_row_reports_fragment_count_above_one);
  return UNITY_END();
}
