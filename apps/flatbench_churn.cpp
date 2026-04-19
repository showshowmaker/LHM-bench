#include "flatbench/flatlike_backend.h"
#include "flatbench/stats.h"
#include "flatbench/workload.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct CliOptions {
    size_t steady_keys{256};
    size_t iterations{5000};
    size_t leaf_capacity{512};
    std::string mode{"oscillate"};
};

bool ParseArgs(int argc, char** argv, CliOptions* options, std::string* error) {
    if (!options) {
        if (error) {
            *error = "cli options output is null";
        }
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        auto require_value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return std::string(argv[++i]);
        };
        if (arg == "--steady-keys") {
            options->steady_keys = static_cast<size_t>(std::stoul(require_value("--steady-keys")));
        } else if (arg == "--iterations") {
            options->iterations = static_cast<size_t>(std::stoul(require_value("--iterations")));
        } else if (arg == "--leaf-capacity") {
            options->leaf_capacity = static_cast<size_t>(std::stoul(require_value("--leaf-capacity")));
        } else if (arg == "--mode") {
            options->mode = require_value("--mode");
        } else {
            if (error) {
                *error = "unknown argument: " + arg;
            }
            return false;
        }
    }
    if (options->mode != "stable" && options->mode != "oscillate") {
        if (error) {
            *error = "--mode must be stable or oscillate";
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        CliOptions options;
        std::string error;
        if (!ParseArgs(argc, argv, &options, &error)) {
            std::cerr << "flatbench_churn: " << error << '\n';
            return 1;
        }

        std::vector<nsbench::NamespaceRecord> records;
        nsbench::NamespaceRecord stable_candidate;
        nsbench::NamespaceRecord oscillating_candidate;
        if (!flatbench::BuildChurnWorkload(options.steady_keys,
                                           &records,
                                           &stable_candidate,
                                           &oscillating_candidate,
                                           &error)) {
            std::cerr << "flatbench_churn: " << error << '\n';
            return 1;
        }

        flatbench::FlatLikeOptions backend_options;
        backend_options.leaf_capacity = options.leaf_capacity;
        flatbench::FlatLikeBackend backend(backend_options);
        if (!backend.Build(records, &error)) {
            std::cerr << "flatbench_churn: " << error << '\n';
            return 1;
        }

        const nsbench::NamespaceRecord& candidate =
            (options.mode == "stable") ? stable_candidate : oscillating_candidate;

        std::vector<uint64_t> insert_latencies;
        std::vector<uint64_t> erase_latencies;
        insert_latencies.reserve(options.iterations);
        erase_latencies.reserve(options.iterations);

        backend.ResetCounters();
        for (size_t i = 0; i < options.iterations; ++i) {
            flatbench::OpStats insert_stats;
            const std::chrono::steady_clock::time_point insert_begin = std::chrono::steady_clock::now();
            if (!backend.Insert(candidate, &insert_stats, &error)) {
                std::cerr << "flatbench_churn: " << error << '\n';
                return 1;
            }
            const std::chrono::steady_clock::time_point insert_end = std::chrono::steady_clock::now();
            const uint64_t insert_ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(insert_end - insert_begin).count());
            insert_latencies.push_back(std::max<uint64_t>(
                insert_ns, insert_stats.lock_wait_ns + insert_stats.lock_hold_ns));

            flatbench::OpStats erase_stats;
            const std::chrono::steady_clock::time_point erase_begin = std::chrono::steady_clock::now();
            if (!backend.Erase(candidate.prepared, &erase_stats, &error)) {
                std::cerr << "flatbench_churn: " << error << '\n';
                return 1;
            }
            const std::chrono::steady_clock::time_point erase_end = std::chrono::steady_clock::now();
            const uint64_t erase_ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(erase_end - erase_begin).count());
            erase_latencies.push_back(std::max<uint64_t>(
                erase_ns, erase_stats.lock_wait_ns + erase_stats.lock_hold_ns));
        }

        const flatbench::LatencySummary insert_summary = flatbench::SummarizeLatencies(insert_latencies);
        const flatbench::LatencySummary erase_summary = flatbench::SummarizeLatencies(erase_latencies);
        const flatbench::FlatInternalCounters counters = backend.Counters();

        std::cout << "mode,iterations,insert_avg_ns,insert_p95_ns,insert_p99_ns,erase_avg_ns,erase_p95_ns,"
                     "erase_p99_ns,prefix_change_count,node_prefix_recompute,prefix_expand_bytes,"
                     "prefix_shrink_bytes,suffix_expand_bytes,suffix_shrink_bytes,node_split,node_merge\n";
        std::cout << options.mode << ','
                  << options.iterations << ','
                  << insert_summary.avg_ns << ','
                  << insert_summary.p95_ns << ','
                  << insert_summary.p99_ns << ','
                  << erase_summary.avg_ns << ','
                  << erase_summary.p95_ns << ','
                  << erase_summary.p99_ns << ','
                  << counters.prefix_change_count << ','
                  << counters.node_prefix_recompute << ','
                  << counters.prefix_expand_bytes << ','
                  << counters.prefix_shrink_bytes << ','
                  << counters.suffix_expand_bytes << ','
                  << counters.suffix_shrink_bytes << ','
                  << counters.node_split << ','
                  << counters.node_merge << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "flatbench_churn error: " << ex.what() << '\n';
        return 1;
    }
}
