#include "cluster_report.h"

#include <cstdio>
#include <map>

ClusterReporter::ClusterReporter(const ClusterReportConfig& config) : config_(config) {}

void ClusterReporter::addEvent(const Event& event) {
  if (history_.size() >= config_.maxEvents) {
    history_.erase(history_.begin());
  }
  history_.push_back({event.timestampMicros, event.direction});
}

ClusterReport ClusterReporter::buildReport(const std::vector<LabeledEvent>& labels,
                                           uint32_t nowMicros) const {
  ClusterReport report;

  // Indices into labels/history_, grouped by cluster id, preserving the
  // chronological order both containers share; std::map keeps groups in
  // ascending clusterId order for free. Noise (-1) is not a device and is
  // dropped here.
  std::map<int32_t, std::vector<size_t>> indicesByCluster;
  const size_t count = labels.size() < history_.size() ? labels.size() : history_.size();
  for (size_t i = 0; i < count; ++i) {
    if (labels[i].clusterId < 0) continue;
    indicesByCluster[labels[i].clusterId].push_back(i);
  }

  for (const auto& entry : indicesByCluster) {
    const int32_t clusterId = entry.first;
    const std::vector<size_t>& indices = entry.second;

    PerClusterStats stats{};
    stats.clusterId = clusterId;

    float clusterMagnitudeSum = 0.0f;
    for (size_t i : indices) clusterMagnitudeSum += labels[i].magnitudeVa;
    stats.meanApparentPowerClusterVa = clusterMagnitudeSum / static_cast<float>(indices.size());

    // FIFO queue of still-open ON indices, within this cluster only.
    std::vector<size_t> openOnIndices;
    float pairMeanSum = 0.0f;

    for (size_t i : indices) {
      const RetainedEvent& re = history_[i];
      const float magnitudeVa = labels[i].magnitudeVa;

      if (re.direction == Direction::kOn) {
        openOnIndices.push_back(i);
        continue;
      }

      if (openOnIndices.empty()) {
        ++stats.offWithoutOnDiscarded;
        continue;
      }

      const size_t onIndex = openOnIndices.front();
      openOnIndices.erase(openOnIndices.begin());
      const RetainedEvent& onEvent = history_[onIndex];
      const float onMagnitudeVa = labels[onIndex].magnitudeVa;

      const float pairMeanVa = (onMagnitudeVa + magnitudeVa) / 2.0f;
      const float durationHours = (re.timestampMicros - onEvent.timestampMicros) / 1e6f / 3600.0f;

      ++stats.completeCycles;
      stats.operatingTimeMicros += re.timestampMicros - onEvent.timestampMicros;
      stats.apparentEnergyVah += pairMeanVa * durationHours;
      pairMeanSum += pairMeanVa;
      stats.cycles.push_back({onEvent.timestampMicros, re.timestampMicros, onMagnitudeVa,
                              magnitudeVa, /*truncated=*/false});
    }

    // Anything still open when the report is built is truncated: closed at
    // nowMicros, using the ON's own magnitude alone since there is no OFF
    // magnitude to average with.
    for (size_t onIndex : openOnIndices) {
      const RetainedEvent& onEvent = history_[onIndex];
      const float onMagnitudeVa = labels[onIndex].magnitudeVa;
      const float durationHours = (nowMicros - onEvent.timestampMicros) / 1e6f / 3600.0f;

      ++stats.truncatedCycles;
      stats.truncatedApparentEnergyVah += onMagnitudeVa * durationHours;
      stats.cycles.push_back({onEvent.timestampMicros, nowMicros, onMagnitudeVa, 0.0f,
                              /*truncated=*/true});
    }

    stats.meanApparentPowerPairMeanVa =
        stats.completeCycles > 0 ? pairMeanSum / static_cast<float>(stats.completeCycles) : 0.0f;

    report.clusters.push_back(stats);
  }

  return report;
}

float estimateCostReais(float apparentEnergyVah, float powerFactorMagnitude,
                        float tariffReaisPerKwh) {
  const float realEnergyKwh = apparentEnergyVah * powerFactorMagnitude / 1000.0f;
  return realEnergyKwh * tariffReaisPerKwh;
}

namespace {

std::string formatFixed(float value, int decimals) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
  return buffer;
}

// Wraps a field in double quotes, doubling any embedded quote, so a value
// carrying commas stays one CSV field. Every power-factor source string does
// carry one, and without this the row silently gains a column.
std::string csvQuoted(const std::string& field) {
  std::string out = "\"";
  for (const char c : field) {
    if (c == '"') {
      out += '"';
    }
    out += c;
  }
  out += '"';
  return out;
}

std::string formatHms(uint32_t elapsedMicros) {
  const uint32_t totalSeconds = elapsedMicros / 1000000u;
  const uint32_t hours = totalSeconds / 3600u;
  const uint32_t minutes = (totalSeconds % 3600u) / 60u;
  const uint32_t seconds = totalSeconds % 60u;
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours, minutes, seconds);
  return buffer;
}

const PowerFactorCategory* findAssignment(
    int32_t clusterId, const std::vector<ClusterPowerFactorAssignment>& assignments) {
  for (const ClusterPowerFactorAssignment& assignment : assignments) {
    if (assignment.clusterId == clusterId) return &assignment.category;
  }
  return nullptr;
}

}  // namespace

std::string formatClusterReportConsole(
    const ClusterReport& report, float tariffReaisPerKwh,
    const std::vector<ClusterPowerFactorAssignment>& assignments) {
  std::string out = "tariff: R$ " + formatFixed(tariffReaisPerKwh, 4) + "/kWh   (" +
                    std::string(kTariffSource) + ")\n";

  for (const PerClusterStats& stats : report.clusters) {
    out += "Device " + std::to_string(stats.clusterId) + " (cluster " +
           std::to_string(stats.clusterId) + ")";

    if (stats.completeCycles == 0) {
      out += "   detected, no complete cycle\n";
    } else {
      out += "   cycles: " + std::to_string(stats.completeCycles) +
             "   operating time: " + formatHms(stats.operatingTimeMicros) + "\n";
      out += "  mean apparent power: " + formatFixed(stats.meanApparentPowerPairMeanVa, 1) +
             " VA (pair mean) / " + formatFixed(stats.meanApparentPowerClusterVa, 1) +
             " VA (cluster mean)\n";
      out += "  apparent energy: " + formatFixed(stats.apparentEnergyVah, 4) + " VA*h\n";

      const PowerFactorCategory* assigned = findAssignment(stats.clusterId, assignments);
      if (assigned != nullptr) {
        const float cost =
            estimateCostReais(stats.apparentEnergyVah, assigned->magnitude, tariffReaisPerKwh);
        out += "  estimated cost: R$ " + formatFixed(cost, 4) +
               "   (assumed power factor: " + formatFixed(assigned->magnitude, 2) +
               ", source: " + assigned->source + ", " + assigned->name + ")\n";
      }

      const float costMin = estimateCostReais(stats.apparentEnergyVah,
                                              kPowerFactorRangeMinMagnitude, tariffReaisPerKwh);
      const float costMax = estimateCostReais(stats.apparentEnergyVah,
                                              kPowerFactorRangeMaxMagnitude, tariffReaisPerKwh);
      out += "  cost range (unsupervised, |PF| " + formatFixed(kPowerFactorRangeMinMagnitude, 2) +
             "-" + formatFixed(kPowerFactorRangeMaxMagnitude, 2) + "): R$ " +
             formatFixed(costMin, 4) + " - R$ " + formatFixed(costMax, 4) + "\n";
    }

    if (stats.truncatedCycles > 0) {
      out += "  truncated: " + std::to_string(stats.truncatedCycles) + " cycle(s), " +
             formatFixed(stats.truncatedApparentEnergyVah, 4) + " VA*h open at end of capture\n";
    }
    out += "  unmatched: " + std::to_string(stats.offWithoutOnDiscarded) +
           " off-without-on discarded\n";
  }

  return out;
}

std::string formatClusterReportCsv(const ClusterReport& report, float tariffReaisPerKwh,
                                   const std::vector<ClusterPowerFactorAssignment>& assignments) {
  std::string out =
      "cluster,complete_cycles,operating_time_s,mean_power_pair_va,mean_power_cluster_va,"
      "apparent_energy_vah,truncated_cycles,truncated_energy_vah,off_without_on,"
      "power_factor_used,power_factor_source,estimated_cost_reais,cost_range_min_reais,"
      "cost_range_max_reais,tariff_reais_per_kwh,tariff_source\n";

  for (const PerClusterStats& stats : report.clusters) {
    out += std::to_string(stats.clusterId) + "," + std::to_string(stats.completeCycles) + "," +
           formatFixed(stats.operatingTimeMicros / 1e6f, 3) + "," +
           formatFixed(stats.meanApparentPowerPairMeanVa, 4) + "," +
           formatFixed(stats.meanApparentPowerClusterVa, 4) + "," +
           formatFixed(stats.apparentEnergyVah, 6) + "," + std::to_string(stats.truncatedCycles) +
           "," + formatFixed(stats.truncatedApparentEnergyVah, 6) + "," +
           std::to_string(stats.offWithoutOnDiscarded) + ",";

    if (stats.completeCycles == 0) {
      out += ",,,,," + formatFixed(tariffReaisPerKwh, 4) + "," + csvQuoted(kTariffSource) + "\n";
      continue;
    }

    const PowerFactorCategory* assigned = findAssignment(stats.clusterId, assignments);
    if (assigned != nullptr) {
      const float cost =
          estimateCostReais(stats.apparentEnergyVah, assigned->magnitude, tariffReaisPerKwh);
      out += formatFixed(assigned->magnitude, 2) + "," + csvQuoted(assigned->source) + "," +
             formatFixed(cost, 4) + ",";
    } else {
      out += ",,,";
    }

    const float costMin = estimateCostReais(stats.apparentEnergyVah, kPowerFactorRangeMinMagnitude,
                                            tariffReaisPerKwh);
    const float costMax = estimateCostReais(stats.apparentEnergyVah, kPowerFactorRangeMaxMagnitude,
                                            tariffReaisPerKwh);
    out += formatFixed(costMin, 4) + "," + formatFixed(costMax, 4) + "," +
           formatFixed(tariffReaisPerKwh, 4) + "," + csvQuoted(kTariffSource) + "\n";
  }

  return out;
}
