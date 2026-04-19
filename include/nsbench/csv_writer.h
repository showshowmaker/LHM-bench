#pragma once

#include "nsbench/bench_runner.h"

#include <string>
#include <vector>

namespace nsbench {

bool WriteBenchmarkReportsCsv(const std::vector<BenchmarkReport>& reports,
                              const std::string& csv_path,
                              std::string* error);

bool AppendBenchmarkReportsCsv(const std::vector<BenchmarkReport>& reports,
                               const std::string& csv_path,
                               bool write_header,
                               std::string* error);

}  // namespace nsbench
