#pragma once

#include "nsbench/common_types.h"
#include "nsbench/resolver.h"

#include <string>
#include <vector>

namespace nsbench {

struct RunOptions {
    uint32_t warmup_queries{10000};
    uint32_t repeats{5};
    bool check_correctness{true};
};

struct RepeatResult {
    uint64_t query_count{0};
    double avg_ns{0.0};
    double p50_ns{0.0};
    double p95_ns{0.0};
    double p99_ns{0.0};
    double throughput_qps{0.0};
    double avg_depth{0.0};
    double avg_component_steps{0.0};
    double avg_index_steps{0.0};
    double avg_steps{0.0};
};

struct BenchmarkReport {
    std::string backend;
    std::string dataset_name;
    std::string manifest_path;
    uint32_t depth{0};
    std::string query_kind;
    std::vector<RepeatResult> repeats;
};

class BenchRunner {
public:
    bool Run(IPathResolver* resolver,
             uint32_t depth,
             const std::string& query_kind,
             const std::vector<QueryRecord>& queries,
             const RunOptions& options,
             BenchmarkReport* report,
             std::string* error) const;

private:
    bool RunOne(IPathResolver* resolver,
                const std::vector<QueryRecord>& queries,
                bool check_correctness,
                RepeatResult* result,
                std::string* error) const;

    static void ComputePercentiles(std::vector<uint64_t>* latencies_ns,
                                   double* p50_ns,
                                   double* p95_ns,
                                   double* p99_ns);
};

}  // namespace nsbench
