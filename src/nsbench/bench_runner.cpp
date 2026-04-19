#include "nsbench/bench_runner.h"

#include <algorithm>
#include <chrono>

namespace nsbench {

bool BenchRunner::Run(IPathResolver* resolver,
                      uint32_t depth,
                      const std::string& query_kind,
                      const std::vector<QueryRecord>& queries,
                      const RunOptions& options,
                      BenchmarkReport* report,
                      std::string* error) const {
    if (!resolver || !report) {
        if (error) {
            *error = "invalid benchmark run args";
        }
        return false;
    }

    if (options.warmup_queries != 0) {
        const size_t warmup_count = std::min<size_t>(options.warmup_queries, queries.size());
        std::vector<QueryRecord> warmup(queries.begin(), queries.begin() + warmup_count);
        if (!resolver->Warmup(warmup, error)) {
            return false;
        }
    }

    BenchmarkReport out;
    out.backend = resolver->Name();
    out.depth = depth;
    out.query_kind = query_kind;
    out.repeats.reserve(options.repeats);
    for (uint32_t i = 0; i < options.repeats; ++i) {
        RepeatResult result;
        if (!RunOne(resolver, queries, options.check_correctness, &result, error)) {
            return false;
        }
        out.repeats.push_back(result);
    }
    *report = std::move(out);
    if (error) {
        error->clear();
    }
    return true;
}

bool BenchRunner::RunOne(IPathResolver* resolver,
                         const std::vector<QueryRecord>& queries,
                         bool check_correctness,
                         RepeatResult* result,
                         std::string* error) const {
    if (!resolver || !result) {
        if (error) {
            *error = "invalid benchmark repeat args";
        }
        return false;
    }

    std::vector<uint64_t> latencies_ns;
    latencies_ns.reserve(queries.size());
    double total_depth = 0.0;
    double total_component_steps = 0.0;
    double total_index_steps = 0.0;
    double total_steps = 0.0;
    const auto begin = std::chrono::steady_clock::now();
    for (const QueryRecord& query : queries) {
        ResolveResult resolved;
        const auto q_begin = std::chrono::steady_clock::now();
        if (!resolver->Resolve(query.prepared, &resolved, error)) {
            return false;
        }
        const auto q_end = std::chrono::steady_clock::now();
        latencies_ns.push_back(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(q_end - q_begin).count()));
        total_depth += resolved.depth;
        total_component_steps += resolved.component_steps;
        total_index_steps += resolved.index_steps;
        total_steps += resolved.steps;

        if (check_correctness) {
            if (resolved.found != query.expect_found) {
                if (error) {
                    *error = "resolve correctness mismatch for path: " + query.path;
                }
                return false;
            }
            if (query.expect_found && resolved.inode_id != query.expected_inode_id) {
                if (error) {
                    *error = "resolve inode mismatch for path: " + query.path;
                }
                return false;
            }
        }
    }
    const auto end = std::chrono::steady_clock::now();
    const double elapsed_seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - begin).count();

    RepeatResult out;
    out.query_count = latencies_ns.size();
    if (!latencies_ns.empty()) {
        uint64_t sum = 0;
        for (uint64_t latency : latencies_ns) {
            sum += latency;
        }
        out.avg_ns = static_cast<double>(sum) / static_cast<double>(latencies_ns.size());
        ComputePercentiles(&latencies_ns, &out.p50_ns, &out.p95_ns, &out.p99_ns);
        out.avg_depth = total_depth / static_cast<double>(latencies_ns.size());
        out.avg_component_steps = total_component_steps / static_cast<double>(latencies_ns.size());
        out.avg_index_steps = total_index_steps / static_cast<double>(latencies_ns.size());
        out.avg_steps = total_steps / static_cast<double>(latencies_ns.size());
    }
    if (elapsed_seconds > 0.0) {
        out.throughput_qps = static_cast<double>(queries.size()) / elapsed_seconds;
    }

    *result = out;
    if (error) {
        error->clear();
    }
    return true;
}

void BenchRunner::ComputePercentiles(std::vector<uint64_t>* latencies_ns,
                                     double* p50_ns,
                                     double* p95_ns,
                                     double* p99_ns) {
    if (!latencies_ns || latencies_ns->empty()) {
        *p50_ns = 0.0;
        *p95_ns = 0.0;
        *p99_ns = 0.0;
        return;
    }

    std::sort(latencies_ns->begin(), latencies_ns->end());
    auto pick = [&](double ratio) -> double {
        const size_t index = static_cast<size_t>(ratio * static_cast<double>(latencies_ns->size() - 1));
        return static_cast<double>((*latencies_ns)[index]);
    };
    *p50_ns = pick(0.50);
    *p95_ns = pick(0.95);
    *p99_ns = pick(0.99);
}

}  // namespace nsbench
