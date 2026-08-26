#include "cluster_report_output.h"

#include <Arduino.h>

void printClusterReportConsole(const ClusterReport& report, float tariffReaisPerKwh,
                               const std::vector<ClusterPowerFactorAssignment>& assignments) {
  Serial.print(formatClusterReportConsole(report, tariffReaisPerKwh, assignments).c_str());
}

void printClusterReportCsv(const ClusterReport& report, float tariffReaisPerKwh,
                           const std::vector<ClusterPowerFactorAssignment>& assignments) {
  Serial.print(formatClusterReportCsv(report, tariffReaisPerKwh, assignments).c_str());
}
