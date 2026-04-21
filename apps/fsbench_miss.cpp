#include "fsbench/cache_control.h"
#include "fsbench/csv_writer.h"
#include "fsbench/ext4_backend.h"
#include "fsbench/lhm_backend.h"
#include "fsbench/workload.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>

namespace {

struct CliOptions {
    std::string backend = "both";
    std::string mode = "warm";
    std::string op = "lookup";
    std::string query_kind = "positive";
    std::string mount_root;
    std::string output_csv = "results/fsbench_miss.csv";
    uint32_t depth = 8;
    uint32_t siblings_per_dir = 16;
    uint32_t files_per_leaf = 64;
    uint32_t positive_queries = 10000;
    uint32_t negative_queries = 10000;
    uint32_t seed = 1;
};

fsbench::OpKind ParseOp(const std::string& value) {
    if (value == "lookup") {
        return fsbench::OpKind::kLookupOnly;
    }
    if (value == "read4k") {
        return fsbench::OpKind::kOpenRead4K;
    }
    if (value == "write4k") {
        return fsbench::OpKind::kOpenWrite4K;
    }
    if (value == "negative_lookup") {
        return fsbench::OpKind::kNegativeLookup;
    }
    throw std::runtime_error("unsupported op: " + value);
}

bool ParseArgs(int argc, char** argv, CliOptions* out, std::string* error) {
    if (!out) {
        if (error) {
            *error = "cli output is null";
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

        if (arg == "--backend") {
            out->backend = require_value("--backend");
        } else if (arg == "--mode") {
            out->mode = require_value("--mode");
        } else if (arg == "--op") {
            out->op = require_value("--op");
        } else if (arg == "--query-kind") {
            out->query_kind = require_value("--query-kind");
        } else if (arg == "--mount-root") {
            out->mount_root = require_value("--mount-root");
        } else if (arg == "--output-csv") {
            out->output_csv = require_value("--output-csv");
        } else if (arg == "--depth") {
            out->depth = static_cast<uint32_t>(std::stoul(require_value("--depth")));
        } else if (arg == "--siblings-per-dir") {
            out->siblings_per_dir = static_cast<uint32_t>(std::stoul(require_value("--siblings-per-dir")));
        } else if (arg == "--files-per-leaf") {
            out->files_per_leaf = static_cast<uint32_t>(std::stoul(require_value("--files-per-leaf")));
        } else if (arg == "--positive-queries") {
            out->positive_queries = static_cast<uint32_t>(std::stoul(require_value("--positive-queries")));
        } else if (arg == "--negative-queries") {
            out->negative_queries = static_cast<uint32_t>(std::stoul(require_value("--negative-queries")));
        } else if (arg == "--seed") {
            out->seed = static_cast<uint32_t>(std::stoul(require_value("--seed")));
        } else {
            if (error) {
                *error = "unknown argument: " + arg;
            }
            return false;
        }
    }

    if ((out->backend == "ext4" || out->backend == "both") && out->mount_root.empty()) {
        if (error) {
            *error = "--mount-root is required for ext4 backend";
        }
        return false;
    }
    if (out->backend != "ext4" && out->backend != "lhm" && out->backend != "both") {
        if (error) {
            *error = "--backend must be one of: ext4, lhm, both";
        }
        return false;
    }
    if (out->mode != "warm" && out->mode != "cold_dropcache") {
        if (error) {
            *error = "--mode must be one of: warm, cold_dropcache";
        }
        return false;
    }
    if (out->query_kind != "positive" && out->query_kind != "negative") {
        if (error) {
            *error = "--query-kind must be one of: positive, negative";
        }
        return false;
    }

    try {
        (void) ParseOp(out->op);
    } catch (const std::exception& ex) {
        if (error) {
            *error = ex.what();
        }
        return false;
    }

    if (error) {
        error->clear();
    }
    return true;
}

double Percentile(std::vector<uint64_t> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    const size_t index = static_cast<size_t>(std::ceil((p / 100.0) * values.size())) - 1;
    const size_t clipped = std::min(index, values.size() - 1);
    std::nth_element(values.begin(), values.begin() + clipped, values.end());
    return static_cast<double>(values[clipped]);
}

bool PrepareBackend(fsbench::IPathBackend* backend,
                    const fsbench::WorkloadData& workload,
                    std::string* error) {
    return backend->Build(workload.entries, error);
}

const std::vector<fsbench::Query>& SelectQueries(const fsbench::WorkloadData& workload,
                                                 const std::string& kind) {
    return kind == "negative" ? workload.negative_queries : workload.positive_queries;
}

bool RunQueries(fsbench::IPathBackend* backend,
                const std::vector<fsbench::Query>& queries,
                fsbench::OpKind op,
                std::vector<uint64_t>* latencies,
                uint64_t* success_count,
                uint64_t* total_bytes,
                std::string* error) {
    latencies->clear();
    *success_count = 0;
    *total_bytes = 0;

    for (const fsbench::Query& query : queries) {
        fsbench::OpResult result;
        if (!backend->Run(query.prepared, op, &result, error)) {
            return false;
        }
        latencies->push_back(result.latency_ns);
        if (result.ok == query.expect_found) {
            ++(*success_count);
        }
        *total_bytes += result.bytes;
    }
    return true;
}

bool RunOne(fsbench::IPathBackend* backend,
            const CliOptions& options,
            const fsbench::WorkloadData& workload,
            fsbench::MissResultRow* row,
            std::string* error) {
    if (!row) {
        if (error) {
            *error = "miss result row output is null";
        }
        return false;
    }

    fsbench::CacheController cache_controller;
    if (options.mode == "cold_dropcache" && std::string(backend->Name()) == "ext4") {
        if (!cache_controller.DropAllCaches(error)) {
            return false;
        }
    }

    const std::vector<fsbench::Query>& queries = SelectQueries(workload, options.query_kind);
    std::vector<uint64_t> latencies;
    uint64_t success_count = 0;
    uint64_t total_bytes = 0;
    if (!RunQueries(backend,
                    queries,
                    ParseOp(options.op),
                    &latencies,
                    &success_count,
                    &total_bytes,
                    error)) {
        return false;
    }

    const double avg_ns = latencies.empty()
                              ? 0.0
                              : static_cast<double>(
                                    std::accumulate(latencies.begin(), latencies.end(), uint64_t{0})) /
                                    static_cast<double>(latencies.size());

    row->backend = backend->Name();
    row->mode = options.mode;
    row->op = options.op;
    row->query_kind = options.query_kind;
    row->query_count = static_cast<uint64_t>(queries.size());
    row->file_count = fsbench::CountFiles(workload.entries);
    row->depth = options.depth;
    row->siblings_per_dir = options.siblings_per_dir;
    row->files_per_leaf = options.files_per_leaf;
    row->avg_ns = avg_ns;
    row->p50_ns = Percentile(latencies, 50.0);
    row->p95_ns = Percentile(latencies, 95.0);
    row->p99_ns = Percentile(latencies, 99.0);
    row->avg_bytes = queries.empty() ? 0.0 : static_cast<double>(total_bytes) / queries.size();
    row->success_rate = queries.empty() ? 0.0 : static_cast<double>(success_count) / queries.size();
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        CliOptions options;
        std::string error;
        if (!ParseArgs(argc, argv, &options, &error)) {
            std::cerr << "argument error: " << error << '\n';
            return 1;
        }

        fsbench::WorkloadOptions workload_options;
        workload_options.depth = options.depth;
        workload_options.siblings_per_dir = options.siblings_per_dir;
        workload_options.files_per_leaf = options.files_per_leaf;
        workload_options.positive_queries = options.positive_queries;
        workload_options.negative_queries = options.negative_queries;
        workload_options.seed = options.seed;

        fsbench::WorkloadData workload;
        fsbench::WorkloadBuilder builder;
        if (!builder.Build(workload_options, &workload, &error)) {
            std::cerr << "failed to build workload: " << error << '\n';
            return 1;
        }

        std::vector<fsbench::MissResultRow> rows;

        if (options.backend == "ext4" || options.backend == "both") {
            fsbench::Ext4Backend ext4(fsbench::Ext4BackendOptions{options.mount_root, true, false});
            if (!PrepareBackend(&ext4, workload, &error)) {
                std::cerr << "ext4 workload build failed: " << error << '\n';
                return 1;
            }
            fsbench::MissResultRow row;
            if (!RunOne(&ext4, options, workload, &row, &error)) {
                std::cerr << "ext4 miss benchmark failed: " << error << '\n';
                return 1;
            }
            rows.push_back(std::move(row));
        }

        if (options.backend == "lhm" || options.backend == "both") {
            fsbench::LhmBackend lhm(fsbench::LhmBackendOptions{true});
            if (!PrepareBackend(&lhm, workload, &error)) {
                std::cerr << "lhm workload build failed: " << error << '\n';
                return 1;
            }
            fsbench::MissResultRow row;
            if (!RunOne(&lhm, options, workload, &row, &error)) {
                std::cerr << "lhm miss benchmark failed: " << error << '\n';
                return 1;
            }
            rows.push_back(std::move(row));
        }

        if (!fsbench::WriteMissResults(rows, options.output_csv, &error)) {
            std::cerr << "failed to write csv: " << error << '\n';
            return 1;
        }

        for (const fsbench::MissResultRow& row : rows) {
            std::cout << row.backend
                      << " mode=" << row.mode
                      << " op=" << row.op
                      << " query_kind=" << row.query_kind
                      << " avg_ns=" << row.avg_ns
                      << " p50_ns=" << row.p50_ns
                      << " p99_ns=" << row.p99_ns
                      << " success_rate=" << row.success_rate
                      << '\n';
        }

        std::cout << "wrote miss benchmark results to " << options.output_csv << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fatal error: " << ex.what() << '\n';
        return 1;
    }
}
