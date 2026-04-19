#include "nsbench/csv_writer.h"

#include <fstream>

namespace nsbench {

namespace {

void WriteHeader(std::ofstream* out) {
    *out << "backend,dataset_name,manifest_path,depth,query_kind,repeat,query_count,"
            "avg_ns,p50_ns,p95_ns,p99_ns,throughput_qps,avg_depth,avg_component_steps,"
            "avg_index_steps,avg_steps\n";
}

}  // namespace

bool AppendBenchmarkReportsCsv(const std::vector<BenchmarkReport>& reports,
                               const std::string& csv_path,
                               bool write_header,
                               std::string* error) {
    std::ofstream out(csv_path, write_header ? std::ios::trunc : std::ios::app);
    if (!out) {
        if (error) {
            *error = "failed to open csv output: " + csv_path;
        }
        return false;
    }

    if (write_header) {
        WriteHeader(&out);
    }

    for (const BenchmarkReport& report : reports) {
        for (size_t i = 0; i < report.repeats.size(); ++i) {
            const RepeatResult& repeat = report.repeats[i];
            out << report.backend << ','
                << report.dataset_name << ','
                << report.manifest_path << ','
                << report.depth << ','
                << report.query_kind << ','
                << i << ','
                << repeat.query_count << ','
                << repeat.avg_ns << ','
                << repeat.p50_ns << ','
                << repeat.p95_ns << ','
                << repeat.p99_ns << ','
                << repeat.throughput_qps << ','
                << repeat.avg_depth << ','
                << repeat.avg_component_steps << ','
                << repeat.avg_index_steps << ','
                << repeat.avg_steps << '\n';
        }
    }

    if (!out.good()) {
        if (error) {
            *error = "failed to write csv output: " + csv_path;
        }
        return false;
    }
    return true;
}

bool WriteBenchmarkReportsCsv(const std::vector<BenchmarkReport>& reports,
                              const std::string& csv_path,
                              std::string* error) {
    return AppendBenchmarkReportsCsv(reports, csv_path, true, error);
}

}  // namespace nsbench
