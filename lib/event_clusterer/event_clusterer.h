#pragma once

#include <cstdint>
#include <vector>

// Pure appliance clustering module for the reading system. No Arduino /
// Wire / Serial dependencies, so it builds and is unit tested in the
// `native` environment. Fed merged events' magnitudes (VA), maintains a
// bounded history and assigns each one a DBSCAN cluster label, discovering
// the number of active appliances on its own with no signature database,
// re-clustering the full retained history on every call. That is cheap
// enough at the scale in play here (O(n log n) over a history capped at a
// few hundred readings, called at most once every few seconds by the
// merge window) that a cadence would only add complexity for no benefit.

// Single install configuration block, injected rather than hardcoded.
struct EventClustererConfig {
  float epsilonVa;     // CLUSTER_EPSILON_VA   [VA]  e.g. 12.0
  uint32_t minPoints;  // CLUSTER_MIN_POINTS         e.g. 4, counted including the point itself
  uint32_t maxEvents;  // CLUSTER_MAX_EVENTS         bounded history size
};

// One retained event's magnitude and current cluster assignment: -1 for an
// outlier, else a positive id assigned in order of increasing magnitude.
struct LabeledEvent {
  float magnitudeVa;
  int32_t clusterId;
};

class EventClusterer {
 public:
  explicit EventClusterer(const EventClustererConfig& config);

  // Feeds one merged event's magnitude (VA), evicting the oldest retained
  // event first if the history is already at maxEvents. Returns the newly
  // fed event's own cluster id.
  int32_t addEvent(float magnitudeVa);

  // The full retained history and its current cluster assignment, oldest
  // first. A cluster's membership can shift as later events arrive; that is
  // inherent to re-clustering, not a bug, so this reflects only the labels
  // as of this call. Exposed for inspection beyond the single label
  // addEvent returns for the newest event.
  std::vector<LabeledEvent> labels() const;

 private:
  // One-dimensional DBSCAN over history_, in the algorithm's original
  // convention of counting MinPts including the point itself. In one
  // dimension a sorted point's neighbourhood is a contiguous run, which
  // turns both the core point scan and the density reachability chaining
  // between core points into linear sweeps over the sorted order: an
  // O(n log n) exact equivalent of the definitional O(n^2) neighbourhood
  // scan, not an approximation of it.
  std::vector<LabeledEvent> clusterHistory() const;

  EventClustererConfig config_;
  std::vector<float> history_;
};
