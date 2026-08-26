#include <unity.h>

#include "cluster_report.h"

static ClusterReportConfig makeConfig() {
  ClusterReportConfig c;
  c.maxEvents = 128;
  c.tariffReaisPerKwh = 0.92f;
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

// Feeds `events` into `reporter` and returns the matching LabeledEvent
// vector (one clusterId per event, same order), the shape buildReport
// expects from EventClusterer::labels().
static std::vector<LabeledEvent> feedWithLabels(ClusterReporter& reporter,
                                                const std::vector<Event>& events,
                                                const std::vector<int32_t>& clusterIds) {
  std::vector<LabeledEvent> labels;
  for (size_t i = 0; i < events.size(); ++i) {
    reporter.addEvent(events[i]);
    labels.push_back({events[i].magnitudeVa, clusterIds[i]});
  }
  return labels;
}

// Two interleaved appliances in one cluster (ON_A, ON_B, OFF_first,
// OFF_second): FIFO pairs OFF_first with the oldest still-open ON (A), not
// with the most recently opened one (B). A LIFO implementation would swap
// both pairs, changing the durations (20s/20s under FIFO vs 10s/30s under
// LIFO) and the resulting apparent energy (0.7222 VA*h under FIFO vs
// 0.7167 VA*h under LIFO), even though the two implementations coincide on
// the mean of pair means (65 VA either way) — asserted on the individual
// cycles, not just the aggregate, so the two do not look alike.
void test_fifo_pairing_on_interleaved_two_appliance_cluster(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(0.0f, 50.0f, Direction::kOn),    // ON_A
      makeEvent(10.0f, 80.0f, Direction::kOn),   // ON_B
      makeEvent(20.0f, 52.0f, Direction::kOff),  // OFF_first
      makeEvent(30.0f, 78.0f, Direction::kOff),  // OFF_second
  };
  std::vector<int32_t> clusterIds = {1, 1, 1, 1};

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  ClusterReport report = reporter.buildReport(labels, toMicros(40.0f));

  TEST_ASSERT_EQUAL_UINT32(1, report.clusters.size());
  const PerClusterStats& stats = report.clusters[0];

  TEST_ASSERT_EQUAL_UINT32(2, stats.completeCycles);
  TEST_ASSERT_EQUAL_UINT32(2, stats.cycles.size());

  // FIFO: first resolved pair is (ON_A, OFF_first).
  TEST_ASSERT_EQUAL_UINT32(toMicros(0.0f), stats.cycles[0].onTimestampMicros);
  TEST_ASSERT_EQUAL_UINT32(toMicros(20.0f), stats.cycles[0].offTimestampMicros);
  TEST_ASSERT_EQUAL_FLOAT(50.0f, stats.cycles[0].onMagnitudeVa);
  TEST_ASSERT_EQUAL_FLOAT(52.0f, stats.cycles[0].offMagnitudeVa);

  // Second resolved pair is (ON_B, OFF_second).
  TEST_ASSERT_EQUAL_UINT32(toMicros(10.0f), stats.cycles[1].onTimestampMicros);
  TEST_ASSERT_EQUAL_UINT32(toMicros(30.0f), stats.cycles[1].offTimestampMicros);
  TEST_ASSERT_EQUAL_FLOAT(80.0f, stats.cycles[1].onMagnitudeVa);
  TEST_ASSERT_EQUAL_FLOAT(78.0f, stats.cycles[1].offMagnitudeVa);

  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.72222f, stats.apparentEnergyVah);
}

// Operating time equals the sum of (OFF instant - ON instant) over complete
// pairs.
void test_operating_time_sums_complete_pair_durations(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(0.0f, 40.0f, Direction::kOn),
      makeEvent(15.0f, 41.0f, Direction::kOff),
      makeEvent(100.0f, 39.0f, Direction::kOn),
      makeEvent(160.0f, 42.0f, Direction::kOff),
  };
  std::vector<int32_t> clusterIds = {2, 2, 2, 2};

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  ClusterReport report = reporter.buildReport(labels, toMicros(200.0f));

  TEST_ASSERT_EQUAL_UINT32(1, report.clusters.size());
  TEST_ASSERT_EQUAL_UINT32(toMicros(15.0f) + toMicros(60.0f),
                           report.clusters[0].operatingTimeMicros);
}

// Pair magnitude is the mean of that pair's own two magnitudes; the
// cluster mean (every event in the cluster, any direction) is reported
// separately and is not required to match it.
void test_pair_magnitude_is_pair_mean_cluster_mean_reported_separately(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(0.0f, 30.0f, Direction::kOn),
      makeEvent(10.0f, 50.0f, Direction::kOff),
  };
  std::vector<int32_t> clusterIds = {3, 3};

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  ClusterReport report = reporter.buildReport(labels, toMicros(20.0f));

  const PerClusterStats& stats = report.clusters[0];
  TEST_ASSERT_EQUAL_FLOAT(40.0f, stats.meanApparentPowerPairMeanVa);  // (30+50)/2
  TEST_ASSERT_EQUAL_FLOAT(40.0f, stats.meanApparentPowerClusterVa);   // (30+50)/2, coincides here
}

// An OFF with no preceding ON in its cluster is discarded from pairing and
// counted, not silently dropped: the appliance was already running when
// the capture began.
void test_off_without_preceding_on_is_discarded_and_counted(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(0.0f, 45.0f, Direction::kOff),   // orphan OFF, no preceding ON
      makeEvent(10.0f, 40.0f, Direction::kOn),   //
      makeEvent(20.0f, 41.0f, Direction::kOff),  // pairs with the ON above
  };
  std::vector<int32_t> clusterIds = {4, 4, 4};

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  ClusterReport report = reporter.buildReport(labels, toMicros(30.0f));

  const PerClusterStats& stats = report.clusters[0];
  TEST_ASSERT_EQUAL_UINT32(1, stats.offWithoutOnDiscarded);
  TEST_ASSERT_EQUAL_UINT32(1, stats.completeCycles);
}

// An ON still open when the report is built (no OFF by end of capture) is
// closed at nowMicros, flagged truncated, and its energy reported
// separately from the untruncated total.
void test_on_open_at_end_of_capture_is_truncated_and_reported_separately(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(0.0f, 60.0f, Direction::kOn),
      makeEvent(10.0f, 61.0f, Direction::kOff),
      makeEvent(50.0f, 62.0f, Direction::kOn),  // never closes
  };
  std::vector<int32_t> clusterIds = {5, 5, 5};

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  const uint32_t nowMicros = toMicros(3650.0f);  // one hour after the truncated ON
  ClusterReport report = reporter.buildReport(labels, nowMicros);

  const PerClusterStats& stats = report.clusters[0];
  TEST_ASSERT_EQUAL_UINT32(1, stats.completeCycles);
  TEST_ASSERT_EQUAL_UINT32(1, stats.truncatedCycles);
  TEST_ASSERT_EQUAL_UINT32(1, stats.cycles.size() - stats.completeCycles);
  TEST_ASSERT_TRUE(stats.cycles[1].truncated);
  // 62 VA over exactly one hour (50s to 3650s) = 62 VA*h, using the ON's
  // own magnitude since there is no OFF magnitude to average with.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 62.0f, stats.truncatedApparentEnergyVah);
  // Untruncated energy is unaffected: 60.5 VA (mean of 60/61) over 10 s.
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 60.5f * 10.0f / 3600.0f, stats.apparentEnergyVah);
}

// A cluster with no complete pair (only a truncated ON) still gets a
// report entry, but with completeCycles == 0, so the caller can print
// "detected, no complete cycle" and skip cost, per acceptance criteria.
void test_cluster_with_no_complete_pair_has_zero_complete_cycles(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {makeEvent(0.0f, 90.0f, Direction::kOn)};
  std::vector<int32_t> clusterIds = {6};

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  ClusterReport report = reporter.buildReport(labels, toMicros(5.0f));

  TEST_ASSERT_EQUAL_UINT32(1, report.clusters.size());
  TEST_ASSERT_EQUAL_UINT32(0, report.clusters[0].completeCycles);
  TEST_ASSERT_EQUAL_UINT32(1, report.clusters[0].truncatedCycles);

  std::string console = formatClusterReportConsole(report, 0.92f, {});
  TEST_ASSERT_TRUE(console.find("detected, no complete cycle") != std::string::npos);
  TEST_ASSERT_TRUE(console.find("estimated cost") == std::string::npos);
}

// The report is computed over the *final* label, not whatever label an
// event had when it was first fed: a later event relabels an earlier one
// (as EventClusterer's re-clustering does), and buildReport must reflect
// the label passed in at report time, not any earlier one.
void test_report_reflects_final_label_not_original(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(0.0f, 55.0f, Direction::kOn),
      makeEvent(10.0f, 56.0f, Direction::kOff),
  };
  reporter.addEvent(events[0]);
  reporter.addEvent(events[1]);

  // As first labeled (e.g. before a later event merged clusters): both in
  // cluster 7.
  std::vector<LabeledEvent> originalLabels = {{55.0f, 7}, {56.0f, 7}};
  ClusterReport originalReport = reporter.buildReport(originalLabels, toMicros(20.0f));
  TEST_ASSERT_EQUAL_UINT32(1, originalReport.clusters.size());
  TEST_ASSERT_EQUAL_INT32(7, originalReport.clusters[0].clusterId);

  // Final label after re-clustering: relabeled to cluster 9. The report
  // built from the final labels must use 9, not the stale 7.
  std::vector<LabeledEvent> finalLabels = {{55.0f, 9}, {56.0f, 9}};
  ClusterReport finalReport = reporter.buildReport(finalLabels, toMicros(20.0f));
  TEST_ASSERT_EQUAL_UINT32(1, finalReport.clusters.size());
  TEST_ASSERT_EQUAL_INT32(9, finalReport.clusters[0].clusterId);
  TEST_ASSERT_EQUAL_UINT32(1, finalReport.clusters[0].completeCycles);
}

// The assumed power factor is printed alongside every cost figure, with
// its source, and the unsupervised cost range accompanies it regardless of
// whether a category was assigned.
void test_cost_prints_assumed_power_factor_alongside_it(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(0.0f, 800.0f, Direction::kOn),
      makeEvent(3600.0f, 800.0f, Direction::kOff),  // exactly one hour: 800 VA*h
  };
  std::vector<int32_t> clusterIds = {10, 10};

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  ClusterReport report = reporter.buildReport(labels, toMicros(3600.0f));

  std::vector<ClusterPowerFactorAssignment> assignments = {{10, kPowerFactorSandwichPress}};
  std::string console = formatClusterReportConsole(report, 0.92f, assignments);

  TEST_ASSERT_TRUE(console.find("estimated cost") != std::string::npos);
  TEST_ASSERT_TRUE(console.find("assumed power factor: 0.99") != std::string::npos);
  TEST_ASSERT_TRUE(console.find(kPowerFactorSandwichPress.source) != std::string::npos);
  // 800 VA*h * 0.99 / 1000 * 0.92 R$/kWh.
  const float expectedCost = 800.0f * 0.99f / 1000.0f * 0.92f;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expectedCost, estimateCostReais(800.0f, 0.99f, 0.92f));

  // The unsupervised range is present even though a category was assigned.
  TEST_ASSERT_TRUE(console.find("cost range") != std::string::npos);

  // A cluster with no assignment still gets the range, no point estimate.
  std::string unassignedConsole = formatClusterReportConsole(report, 0.92f, {});
  TEST_ASSERT_TRUE(unassignedConsole.find("estimated cost") == std::string::npos);
  TEST_ASSERT_TRUE(unassignedConsole.find("cost range") != std::string::npos);
}

// CSV form carries the same figures as the console block, header included.
void test_csv_form_carries_the_same_figures(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(0.0f, 100.0f, Direction::kOn),
      makeEvent(3600.0f, 100.0f, Direction::kOff),
  };
  std::vector<int32_t> clusterIds = {11, 11};

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  ClusterReport report = reporter.buildReport(labels, toMicros(3600.0f));

  std::string csv = formatClusterReportCsv(report, 0.92f, {{11, kPowerFactorFan}});
  TEST_ASSERT_TRUE(csv.find("cluster,complete_cycles") == 0);
  TEST_ASSERT_TRUE(csv.find("11,1,") != std::string::npos);
  TEST_ASSERT_TRUE(csv.find("0.94") != std::string::npos);
}

// Realistic fixture: the charger's cluster from the confirmation session
// (docs/measurements/event-clustering-confirm-cluster-map.csv, cluster 2),
// 5 off-events and 2 merged on-events, all clean single or fused
// transitions. FIFO pairs each on with the next off in time; three offs
// have no preceding on in this fixture (the charger's on-transitions that
// under-fused/went undetected in that session, per the confirm write-up),
// so they must be discarded and counted, not silently dropped.
void test_measured_charger_cluster_fixture_from_confirm_session(void) {
  ClusterReporter reporter(makeConfig());

  std::vector<Event> events = {
      makeEvent(647.020508f, 82.2f, Direction::kOn),   // item 3, rep 1 on, fragments=2
      makeEvent(688.019653f, 91.7f, Direction::kOff),  // rep 1 off
      makeEvent(707.019714f, 93.1f,
                Direction::kOff),  // rep 2 off (rep 2 on under-fused, off scope)
      makeEvent(728.020569f, 96.7f, Direction::kOff),   // rep 3 off
      makeEvent(736.020264f, 100.7f, Direction::kOn),   // rep 5 on, fragments=2
      makeEvent(881.020081f, 98.5f, Direction::kOff),   // rep 4 off
      makeEvent(930.020203f, 100.1f, Direction::kOff),  // rep 5 off
  };
  std::vector<int32_t> clusterIds(events.size(), 2);

  std::vector<LabeledEvent> labels = feedWithLabels(reporter, events, clusterIds);
  ClusterReport report = reporter.buildReport(labels, toMicros(1000.0f));

  TEST_ASSERT_EQUAL_UINT32(1, report.clusters.size());
  const PerClusterStats& stats = report.clusters[0];

  // 7 events, 2 on: FIFO pairs each on with the very next off, leaving the
  // remaining 3 offs (2nd, 3rd, 4th in arrival order after the first pair
  // closes) without a preceding on.
  TEST_ASSERT_EQUAL_UINT32(2, stats.completeCycles);
  TEST_ASSERT_EQUAL_UINT32(3, stats.offWithoutOnDiscarded);
  TEST_ASSERT_EQUAL_UINT32(0, stats.truncatedCycles);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fifo_pairing_on_interleaved_two_appliance_cluster);
  RUN_TEST(test_operating_time_sums_complete_pair_durations);
  RUN_TEST(test_pair_magnitude_is_pair_mean_cluster_mean_reported_separately);
  RUN_TEST(test_off_without_preceding_on_is_discarded_and_counted);
  RUN_TEST(test_on_open_at_end_of_capture_is_truncated_and_reported_separately);
  RUN_TEST(test_cluster_with_no_complete_pair_has_zero_complete_cycles);
  RUN_TEST(test_report_reflects_final_label_not_original);
  RUN_TEST(test_cost_prints_assumed_power_factor_alongside_it);
  RUN_TEST(test_csv_form_carries_the_same_figures);
  RUN_TEST(test_measured_charger_cluster_fixture_from_confirm_session);
  return UNITY_END();
}
