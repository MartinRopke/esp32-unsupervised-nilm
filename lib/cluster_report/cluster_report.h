#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "event_clusterer.h"
#include "event_detector.h"

// Pairs on/off events within each DBSCAN cluster and turns them into a
// per-cluster operating report: cycle count, operating time, mean apparent
// power, apparent energy, and an estimated cost. No Arduino / Wire / Serial
// dependencies, so it builds and is unit tested in the `native` environment,
// same as Meter, EventDetector, EventMerger and EventClusterer.
//
// Two tiers, kept apart on purpose. Apparent energy (VA*h) is the system's
// own, unsupervised output: always computed, never conditioned on knowing
// what a cluster is. Cost is a derived estimate that consumes a
// human-supplied power-factor category (see PowerFactorCategory); this
// module never guesses which category applies to a cluster. Assigning one
// is the caller's job (ClusterPowerFactorAssignment), done with the same
// human interpretation recorded in docs/measurements/*-cluster-map.csv, not
// an output of the clustering itself. A cluster left unassigned still gets
// the unsupervised cost range (see kPowerFactorRangeMinMagnitude /
// kPowerFactorRangeMaxMagnitude), which needs no human input at all.

// Single install configuration block, injected rather than hardcoded.
struct ClusterReportConfig {
  // Mirrors EventClustererConfig::maxEvents. This module keeps its own
  // bounded history of (direction, dated instant) per retained event,
  // evicted oldest-first at the same bound as EventClusterer, fed by the
  // same addEvent calls in the same order (see ClusterReporter::addEvent).
  // That keeps this module's history index-aligned with
  // EventClusterer::labels() at report time without depending on
  // EventClusterer's own history representation, which exposes magnitude
  // and cluster id only, not direction or timestamp. Cost: 8 bytes/event
  // resident (uint32_t timestamp + int direction) at the cap; maxEvents=128
  // is 1 KB, negligible against the ESP32's RAM.
  uint32_t maxEvents;

  // TARIFF [R$/kWh]. See the flag on kConfig.tariff in src/main.cpp: no
  // sourced value was available when this module was written, so treat
  // whatever this carries as provisional until a dated utility invoice or
  // an ANEEL table backs it.
  float tariffReaisPerKwh;
};

// One load category's power factor, from Hannagan, J.; Woszczeiko, R.;
// Langstaff, T.; Shen, W.; Rodwell, J. 2023. The impact of household
// appliances and devices: consider their reactive power and power factors.
// Sustainability 15(1): 158. Only the magnitude enters the cost conversion;
// `leading` records the source's sign convention (electronic loads inject
// reactive power) for citation fidelity, even though cost math uses
// |magnitude| only.
struct PowerFactorCategory {
  const char* name;
  float magnitude;  // |power factor| used for the point cost estimate, 0..1
  bool leading;     // true: reactive power injected (source's sign for electronic loads)
  const char* source;
};

// Sandwich press, resistive heating: the source's own single reported
// value.
inline constexpr PowerFactorCategory kPowerFactorSandwichPress = {
    "sandwich press (resistive heating)",
    0.99f,
    false,
    "Hannagan et al. 2023, Sustainability 15(1):158",
};

// Fan, motor: the source measured three fans (0.56, 0.70, 0.94); 0.94 is
// used as the representative value.
inline constexpr PowerFactorCategory kPowerFactorFan = {
    "fan (motor)",
    0.94f,
    false,
    "Hannagan et al. 2023, Sustainability 15(1):158",
};

// Laptop charger, switched-mode electronic: the source gives a range
// (0.47-0.63), not a single figure, so the midpoint is used and named as
// such rather than presenting it as a measured point value. The
// unsupervised cost range (kPowerFactorRangeMinMagnitude /
// kPowerFactorRangeMaxMagnitude) is the honest bound for this category
// specifically.
inline constexpr PowerFactorCategory kPowerFactorLaptopCharger = {
    "laptop charger (switched-mode electronic, midpoint of source's 0.47-0.63 range)",
    0.55f,
    true,
    "Hannagan et al. 2023, Sustainability 15(1):158",
};

// What the tariff behind the cost figures is, printed with every report so a
// saved artefact carries its own premise rather than a bare figure in reais.
//
// The tariff is a configuration constant of the installation, like
// mainsVoltage: it is declared, not measured, and the study does not depend
// on its value. It scales every cost identically and therefore cancels out of
// a percentage error, where both the system and the reference use the same
// figure. Any value would date; keeping it configurable is the point.
// Fetching a live or current tariff is a separate problem and out of scope.
//
// Change it with kConfig.tariff in src/main.cpp, and keep the two in step.
inline constexpr const char* kTariffSource =
    "declared installation constant, not a measurement; cost scales with it";

// The extreme |power factor| magnitudes across the whole table above,
// spanning every category regardless of which one, if any, is assigned to
// a cluster. Needs no human input, so a cost range from these bounds can be
// stated for every cluster with a complete pair, assigned or not.
inline constexpr float kPowerFactorRangeMinMagnitude = 0.47f;  // switched-mode electronic, lower
                                                               // bound
inline constexpr float kPowerFactorRangeMaxMagnitude = 1.0f;   // resistive-heating ceiling

// A human-assigned category for one cluster id, supplied by the caller when
// requesting a report (see ClusterReporter::buildReport). The pairing and
// energy computation never consult this; only the cost line does.
struct ClusterPowerFactorAssignment {
  int32_t clusterId;
  PowerFactorCategory category;
};

// One complete or truncated on/off cycle, FIFO-paired within its cluster.
struct Cycle {
  uint32_t onTimestampMicros;
  uint32_t offTimestampMicros;  // only meaningful when !truncated
  float onMagnitudeVa;
  float offMagnitudeVa;  // only meaningful when !truncated
  bool truncated;        // true: no OFF arrived by the report's nowMicros
};

// Per-cluster summary. One entry per positive cluster id that appears in
// the labels fed to buildReport; the noise cluster (-1) never gets an
// entry, since it stands for events DBSCAN rejected as outliers, not a
// device.
struct PerClusterStats {
  int32_t clusterId;

  uint32_t completeCycles;
  uint32_t operatingTimeMicros;  // summed over complete cycles only

  // Mean, across complete cycles, of each cycle's own two-magnitude mean.
  // Degrades gracefully when a cluster holds two appliances and absorbs a
  // load's own instability; see meanApparentPowerClusterVa for the
  // alternative.
  float meanApparentPowerPairMeanVa;
  // Mean magnitude of every event assigned to this cluster (any
  // direction), independent of pairing. Reported alongside the pair mean
  // so the thesis can compare them, not to replace it.
  float meanApparentPowerClusterVa;

  // VA*h over complete cycles only.
  float apparentEnergyVah;

  uint32_t truncatedCycles;
  // VA*h for truncated cycles, kept apart from apparentEnergyVah: an ON
  // still open at nowMicros has no OFF magnitude to average with, so its
  // energy uses the ON magnitude alone and would understate precision if
  // folded into the untruncated total.
  float truncatedApparentEnergyVah;

  // OFF events that arrived with no preceding ON in this cluster: the
  // appliance was already running when the capture began, so there is no
  // start time to pair it with. Discarded from operating-time and energy
  // totals, but counted here rather than silently dropped.
  uint32_t offWithoutOnDiscarded;

  // Every complete cycle (oldest-paired-first, i.e. the FIFO pairing order)
  // followed by every truncated cycle (in ON order), so a caller — a test,
  // in particular — can check individual pairs rather than only the
  // aggregates above. The aggregates are derived from this list; it is not
  // a second source of truth, just exposed for inspection.
  std::vector<Cycle> cycles;
};

struct ClusterReport {
  std::vector<PerClusterStats> clusters;  // in ascending clusterId order
};

class ClusterReporter {
 public:
  explicit ClusterReporter(const ClusterReportConfig& config);

  // Feeds one merged event (post-EventMerger, pre- or post-EventClusterer,
  // order does not matter to this module), evicting the oldest retained
  // event first once history is already at config.maxEvents. Call this
  // once per released merge, in the same order EventClusterer::addEvent is
  // called with that event's magnitude, so the two histories stay
  // index-aligned (see ClusterReportConfig's comment).
  void addEvent(const Event& event);

  // Builds the report over `labels`, EventClusterer's *final* label for
  // every currently-retained event, in the same order and count as this
  // module's own history (guaranteed by construction when addEvent is
  // called in lockstep with EventClusterer::addEvent, per
  // ClusterReportConfig's comment; violating that precondition is
  // undefined). Recomputed fresh from labels on every call: no incremental
  // pairing state carried between reports, so a later re-clustering that
  // relabels an earlier event is reflected immediately, not left stale.
  // nowMicros closes out any cycle still open (see Cycle::truncated).
  ClusterReport buildReport(const std::vector<LabeledEvent>& labels, uint32_t nowMicros) const;

 private:
  struct RetainedEvent {
    uint32_t timestampMicros;
    Direction direction;
  };

  ClusterReportConfig config_;
  std::vector<RetainedEvent> history_;
};

// energyVah * |powerFactorMagnitude| converts apparent to real energy
// (kWh, dividing by 1000), then tariffReaisPerKwh converts to cost.
float estimateCostReais(float apparentEnergyVah, float powerFactorMagnitude,
                        float tariffReaisPerKwh);

// Renders the report as the console block described in
// ISSUE-cluster-pairing-and-cost.md, one paragraph per cluster in ascending
// clusterId order, each ending in a trailing newline. Clusters with no
// complete cycle print "detected, no complete cycle" instead of a cost
// line. `assignments` supplies a human-chosen PowerFactorCategory for
// whichever cluster ids the caller has identified; an unlisted cluster
// still gets the unsupervised cost range, no point estimate.
std::string formatClusterReportConsole(
    const ClusterReport& report, float tariffReaisPerKwh,
    const std::vector<ClusterPowerFactorAssignment>& assignments);

// Renders the same report as CSV rows, header included, one row per
// cluster, in the same column order the console block reports them.
std::string formatClusterReportCsv(const ClusterReport& report, float tariffReaisPerKwh,
                                   const std::vector<ClusterPowerFactorAssignment>& assignments);
