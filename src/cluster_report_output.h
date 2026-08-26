#pragma once

#include <vector>

#include "cluster_report.h"

// Prints the per-cluster operating report to Serial. Depends on Arduino's
// Serial, so this stays in src/ rather than lib/: lib/cluster_report/ keeps
// the actual pairing, cost math and formatting pure and native-testable;
// this is just the I/O wrapper around it.

void printClusterReportConsole(const ClusterReport& report, float tariffReaisPerKwh,
                               const std::vector<ClusterPowerFactorAssignment>& assignments);

void printClusterReportCsv(const ClusterReport& report, float tariffReaisPerKwh,
                           const std::vector<ClusterPowerFactorAssignment>& assignments);
