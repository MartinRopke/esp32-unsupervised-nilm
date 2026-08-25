#include <unity.h>

#include <algorithm>
#include <vector>

#include "event_clusterer.h"

static EventClustererConfig makeConfig(uint32_t maxEvents = 128) {
  EventClustererConfig c;
  c.epsilonVa = 12.0f;
  c.minPoints = 4;
  c.maxEvents = maxEvents;
  return c;
}

// A tight group of exactly 3 events, with nothing else nearby, has each
// point's own-count neighbourhood (including itself) at 3, short of MinPts
// 4, so none becomes a core point and all three are outliers.
void test_group_of_three_with_no_neighbours_yields_all_outliers(void) {
  EventClusterer clusterer(makeConfig());

  clusterer.addEvent(100.0f);
  clusterer.addEvent(103.0f);
  int32_t lastId = clusterer.addEvent(106.0f);

  TEST_ASSERT_EQUAL_INT32(-1, lastId);
  for (const LabeledEvent& e : clusterer.labels()) {
    TEST_ASSERT_EQUAL_INT32(-1, e.clusterId);
  }
}

// MinPts is counted including the point itself: 4 mutually close points
// (each with exactly 3 others within epsilon) are all core, and form one
// cluster at MinPts 4, but the same 4 points are all outliers at MinPts 5,
// since 4 (3 others plus itself) falls short of 5.
void test_min_points_counted_including_the_point_itself(void) {
  const float points[] = {100.0f, 103.0f, 106.0f, 109.0f};

  EventClustererConfig fourConfig = makeConfig();
  fourConfig.minPoints = 4;
  EventClusterer clustererAtFour(fourConfig);
  int32_t lastIdAtFour = -2;
  for (float p : points) lastIdAtFour = clustererAtFour.addEvent(p);
  TEST_ASSERT_TRUE(lastIdAtFour > 0);
  for (const LabeledEvent& e : clustererAtFour.labels()) {
    TEST_ASSERT_EQUAL_INT32(lastIdAtFour, e.clusterId);
  }

  EventClustererConfig fiveConfig = makeConfig();
  fiveConfig.minPoints = 5;
  EventClusterer clustererAtFive(fiveConfig);
  int32_t lastIdAtFive = 0;
  for (float p : points) lastIdAtFive = clustererAtFive.addEvent(p);
  TEST_ASSERT_EQUAL_INT32(-1, lastIdAtFive);
}

// A border point reachable from only one side, within epsilon of a core
// point's neighbourhood but not dense enough to be core itself, still joins
// that core point's cluster.
void test_border_point_joins_cluster_of_adjacent_core_point(void) {
  EventClusterer clusterer(makeConfig());
  // 0, 3, 6, 9 are mutually within epsilon (12) of each other, all core.
  // 20 is within epsilon of 9 (distance 11) but of nothing else, so its own
  // neighbourhood is only {9, 20}, too sparse to be core on its own.
  clusterer.addEvent(0.0f);
  clusterer.addEvent(3.0f);
  clusterer.addEvent(6.0f);
  clusterer.addEvent(9.0f);
  int32_t borderId = clusterer.addEvent(20.0f);

  TEST_ASSERT_TRUE(borderId > 0);
  std::vector<LabeledEvent> labeled = clusterer.labels();
  TEST_ASSERT_EQUAL_size_t(5, labeled.size());
  for (const LabeledEvent& e : labeled) {
    TEST_ASSERT_EQUAL_INT32(borderId, e.clusterId);
  }
}

// Two core points that are themselves further apart than epsilon must stay
// in separate clusters, even when a single shared border point falls
// within epsilon of both. A cluster only forms by chaining core points
// that are each within epsilon of the next core point in the chain; a
// border point in both neighbourhoods is not itself a link in that chain.
void test_two_distant_core_points_stay_separate_despite_shared_border_point(void) {
  EventClusterer clusterer(makeConfig());
  // epsilon = 12. -8, -4, 0 give 0 a neighbourhood of {-8,-4,0,9} (4, core).
  // 19, 22, 25 give 19 a neighbourhood of {9,19,22,25} (4, core). 0 and 19
  // are 19 apart, past epsilon, so they must not chain into one cluster;
  // 9 sits within epsilon of both (distance 9 and 10) without being core
  // itself (its own neighbourhood is only {0,9,19}, 3 points).
  clusterer.addEvent(-8.0f);
  clusterer.addEvent(-4.0f);
  clusterer.addEvent(0.0f);
  clusterer.addEvent(9.0f);
  clusterer.addEvent(19.0f);
  clusterer.addEvent(22.0f);
  int32_t lastId = clusterer.addEvent(25.0f);

  std::vector<LabeledEvent> labeled = clusterer.labels();
  int32_t lowClusterId = -1, highClusterId = -1;
  for (const LabeledEvent& e : labeled) {
    if (e.magnitudeVa == 0.0f) lowClusterId = e.clusterId;
    if (e.magnitudeVa == 19.0f) highClusterId = e.clusterId;
  }

  TEST_ASSERT_TRUE(lowClusterId > 0);
  TEST_ASSERT_TRUE(highClusterId > 0);
  TEST_ASSERT_NOT_EQUAL(lowClusterId, highClusterId);
  TEST_ASSERT_EQUAL_INT32(highClusterId, lastId);
}

// Cluster ids are assigned by increasing magnitude, and the assignment is
// stable regardless of the order events were fed in.
void test_cluster_ids_assigned_by_increasing_magnitude_and_order_independent(void) {
  EventClusterer inOrder(makeConfig());
  for (float p : {40.0f, 44.0f, 48.0f, 52.0f, 95.0f, 98.0f, 101.0f, 104.0f}) {
    inOrder.addEvent(p);
  }

  EventClusterer scrambled(makeConfig());
  for (float p : {98.0f, 44.0f, 104.0f, 40.0f, 95.0f, 52.0f, 101.0f, 48.0f}) {
    scrambled.addEvent(p);
  }

  auto lowClusterId = [](const std::vector<LabeledEvent>& labeled) {
    for (const LabeledEvent& e : labeled) {
      if (e.magnitudeVa < 60.0f) return e.clusterId;
    }
    return -1;
  };
  auto highClusterId = [](const std::vector<LabeledEvent>& labeled) {
    for (const LabeledEvent& e : labeled) {
      if (e.magnitudeVa > 60.0f) return e.clusterId;
    }
    return -1;
  };

  std::vector<LabeledEvent> orderedLabels = inOrder.labels();
  std::vector<LabeledEvent> scrambledLabels = scrambled.labels();

  // The lower-magnitude cluster is id 1, the higher-magnitude one id 2, in
  // both runs, regardless of feed order.
  TEST_ASSERT_EQUAL_INT32(1, lowClusterId(orderedLabels));
  TEST_ASSERT_EQUAL_INT32(2, highClusterId(orderedLabels));
  TEST_ASSERT_EQUAL_INT32(1, lowClusterId(scrambledLabels));
  TEST_ASSERT_EQUAL_INT32(2, highClusterId(scrambledLabels));
}

// Beyond maxEvents, the oldest events leave the history and clustering
// continues on the remainder: with a cap of 4, feeding a 5th event evicts
// the 1st, and the retained history no longer contains its magnitude.
void test_beyond_max_events_oldest_event_is_evicted(void) {
  EventClusterer clusterer(makeConfig(/*maxEvents=*/4));

  clusterer.addEvent(10.0f);
  clusterer.addEvent(20.0f);
  clusterer.addEvent(30.0f);
  clusterer.addEvent(40.0f);
  clusterer.addEvent(50.0f);

  std::vector<LabeledEvent> labeled = clusterer.labels();
  TEST_ASSERT_EQUAL_size_t(4, labeled.size());
  for (const LabeledEvent& e : labeled) {
    TEST_ASSERT_FALSE(e.magnitudeVa == 10.0f);
  }
}

// Fixture: the 39 merged events reproduced from the same 24 Aug 2026
// confirmation-session bench capture as test_event_merger's raw-event
// fixture (sandwich maker, fan, and laptop charger), after merging at the
// 5 s window. At epsilonVa = 12.0, minPoints = 4, this reproduces exactly
// 3 clusters and 6 outliers: the fan, the charger, and the sandwich maker,
// ranging 39-57 VA, 93-100 VA, and 778-806 VA respectively.
static const float kConfirmationSessionMergedMagnitudesVa[] = {
    802.8538f, 790.2127f, 49.7598f,  40.7826f,  804.0106f, 790.8248f, 804.6805f, 797.4584f,
    798.2819f, 794.9993f, 73.3016f,  57.3342f,  128.8974f, 806.4323f, 789.5943f, 802.9792f,
    802.1556f, 49.2624f,  38.508f,   50.9853f,  40.4984f,  51.0423f,  43.1833f,  98.6621f,
    93.1071f,  99.0979f,  100.1393f, 54.5585f,  96.7435f,  838.3987f, 823.9921f, 782.948f,
    778.2045f, 236.7735f, 56.2791f,  588.7202f, 780.5815f, 789.4929f, 786.7841f,
};
static constexpr size_t kConfirmationMergedCount =
    sizeof(kConfirmationSessionMergedMagnitudesVa) /
    sizeof(kConfirmationSessionMergedMagnitudesVa[0]);

void test_confirmation_session_fixture_reproduces_three_clusters_and_six_outliers(void) {
  TEST_ASSERT_EQUAL_size_t(39, kConfirmationMergedCount);

  EventClusterer clusterer(makeConfig());
  for (float magnitude : kConfirmationSessionMergedMagnitudesVa) {
    clusterer.addEvent(magnitude);
  }

  std::vector<LabeledEvent> labeled = clusterer.labels();
  TEST_ASSERT_EQUAL_size_t(39, labeled.size());

  int outlierCount = 0;
  float fanLow = 1e9f, fanHigh = -1e9f;
  float chargerLow = 1e9f, chargerHigh = -1e9f;
  float sandwichLow = 1e9f, sandwichHigh = -1e9f;
  int fanCount = 0, chargerCount = 0, sandwichCount = 0;

  for (const LabeledEvent& e : labeled) {
    if (e.clusterId == -1) {
      ++outlierCount;
    } else if (e.magnitudeVa < 60.0f) {
      ++fanCount;
      fanLow = std::min(fanLow, e.magnitudeVa);
      fanHigh = std::max(fanHigh, e.magnitudeVa);
    } else if (e.magnitudeVa < 200.0f) {
      ++chargerCount;
      chargerLow = std::min(chargerLow, e.magnitudeVa);
      chargerHigh = std::max(chargerHigh, e.magnitudeVa);
    } else {
      ++sandwichCount;
      sandwichLow = std::min(sandwichLow, e.magnitudeVa);
      sandwichHigh = std::max(sandwichHigh, e.magnitudeVa);
    }
  }

  TEST_ASSERT_EQUAL_INT(6, outlierCount);
  TEST_ASSERT_EQUAL_INT(11, fanCount);
  TEST_ASSERT_EQUAL_INT(5, chargerCount);
  TEST_ASSERT_EQUAL_INT(17, sandwichCount);

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.508f, fanLow);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 57.3342f, fanHigh);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 93.1071f, chargerLow);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.1393f, chargerHigh);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 778.2045f, sandwichLow);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 806.4323f, sandwichHigh);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_group_of_three_with_no_neighbours_yields_all_outliers);
  RUN_TEST(test_min_points_counted_including_the_point_itself);
  RUN_TEST(test_border_point_joins_cluster_of_adjacent_core_point);
  RUN_TEST(test_two_distant_core_points_stay_separate_despite_shared_border_point);
  RUN_TEST(test_cluster_ids_assigned_by_increasing_magnitude_and_order_independent);
  RUN_TEST(test_beyond_max_events_oldest_event_is_evicted);
  RUN_TEST(test_confirmation_session_fixture_reproduces_three_clusters_and_six_outliers);
  return UNITY_END();
}
