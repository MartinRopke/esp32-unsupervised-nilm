#include "event_clusterer.h"

#include <algorithm>
#include <numeric>

EventClusterer::EventClusterer(const EventClustererConfig& config) : config_(config) {
  history_.reserve(config_.maxEvents);
}

int32_t EventClusterer::addEvent(float magnitudeVa) {
  if (history_.size() >= config_.maxEvents) {
    history_.erase(history_.begin());
  }
  history_.push_back(magnitudeVa);

  std::vector<LabeledEvent> labeled = clusterHistory();
  return labeled.back().clusterId;
}

std::vector<LabeledEvent> EventClusterer::labels() const { return clusterHistory(); }

std::vector<LabeledEvent> EventClusterer::clusterHistory() const {
  const size_t n = history_.size();
  std::vector<LabeledEvent> result(n);
  if (n == 0) return result;

  std::vector<size_t> order(n);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(),
            [this](size_t a, size_t b) { return history_[a] < history_[b]; });

  // Two-pointer neighbourhood bounds over the sorted magnitudes: lo[i]/hi[i]
  // are the first/last sorted positions within epsilonVa of position i,
  // inclusive. Valid because sorted magnitudes are non-decreasing, so both
  // pointers only ever move forward as i increases.
  std::vector<size_t> lo(n), hi(n);
  size_t loPtr = 0, hiPtr = 0;
  for (size_t i = 0; i < n; ++i) {
    float magnitude = history_[order[i]];
    while (magnitude - history_[order[loPtr]] > config_.epsilonVa) ++loPtr;
    if (hiPtr < i) hiPtr = i;
    while (hiPtr + 1 < n && history_[order[hiPtr + 1]] - magnitude <= config_.epsilonVa) {
      ++hiPtr;
    }
    lo[i] = loPtr;
    hi[i] = hiPtr;
  }

  std::vector<bool> isCore(n);
  for (size_t i = 0; i < n; ++i) {
    isCore[i] = (hi[i] - lo[i] + 1) >= static_cast<size_t>(config_.minPoints);
  }

  // Core points chain into one cluster when consecutive core points (in
  // sorted order) are themselves within epsilonVa: direct density
  // reachability between the two. Because values are sorted, a core point
  // any further down the same chain is automatically within epsilonVa of
  // some earlier core in it too (transitively, through the intermediate
  // cores), so only the gap between consecutive cores needs checking, not
  // every pair. A shared border point that merely sits in both
  // neighbourhoods does not connect two cores that are themselves more than
  // epsilonVa apart: DBSCAN clusters via chains of core points, not via a
  // border point's two independent, unconnected core neighbours.
  std::vector<int32_t> coreClusterId(n, -1);
  int32_t clusterId = 0;
  bool haveOpenChain = false;
  float lastCoreMagnitude = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    if (!isCore[i]) continue;
    float magnitude = history_[order[i]];
    if (!haveOpenChain || magnitude - lastCoreMagnitude > config_.epsilonVa) {
      ++clusterId;
      haveOpenChain = true;
    }
    coreClusterId[i] = clusterId;
    lastCoreMagnitude = magnitude;
  }

  // Nearest core index at or before / at or after each position, so a
  // border or outlier point can be judged against the closest core on
  // either side rather than an arbitrary one.
  std::vector<size_t> prevCore(n), nextCore(n);
  static constexpr size_t kNone = static_cast<size_t>(-1);
  size_t running = kNone;
  for (size_t i = 0; i < n; ++i) {
    if (isCore[i]) running = i;
    prevCore[i] = running;
  }
  running = kNone;
  for (size_t i = n; i-- > 0;) {
    if (isCore[i]) running = i;
    nextCore[i] = running;
  }

  std::vector<int32_t> sortedLabels(n, -1);
  for (size_t i = 0; i < n; ++i) {
    if (isCore[i]) {
      sortedLabels[i] = coreClusterId[i];
      continue;
    }
    float magnitude = history_[order[i]];
    bool havePrev = prevCore[i] != kNone;
    bool haveNext = nextCore[i] != kNone;
    float distPrev = havePrev ? magnitude - history_[order[prevCore[i]]] : -1.0f;
    float distNext = haveNext ? history_[order[nextCore[i]]] - magnitude : -1.0f;
    bool prevInRange = havePrev && distPrev <= config_.epsilonVa;
    bool nextInRange = haveNext && distNext <= config_.epsilonVa;
    if (prevInRange && (!nextInRange || distPrev <= distNext)) {
      sortedLabels[i] = coreClusterId[prevCore[i]];
    } else if (nextInRange) {
      sortedLabels[i] = coreClusterId[nextCore[i]];
    }
  }

  for (size_t i = 0; i < n; ++i) {
    result[order[i]] = {history_[order[i]], sortedLabels[i]};
  }
  return result;
}
