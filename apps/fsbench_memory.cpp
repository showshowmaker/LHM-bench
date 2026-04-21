#include "fsbench/csv_writer.h"
#include "fsbench/ext4_backend.h"
#include "fsbench/lhm_backend.h"
#include "fsbench/workload.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

struct CliOptions {
    std::string backend = "both";
    std::string mount_root;
    std::string output_csv = "results/fsbench_memory.csv";
    uint32_t depth = 8;
    uint32_t siblings_per_dir = 16;
    uint32_t files_per_leaf = 64;
    uint64_t target_file_count = 0;
    uint32_t positive_queries = 10000;
    uint32_t negative_queries = 10000;
    uint32_t seed = 1;
};

fsbench::MemorySnapshot DeltaSnapshot(const fsbench::MemorySnapshot& current,
                                      const fsbench::MemorySnapshot& baseline,
                                      uint64_t file_count) {
    fsbench::MemorySnapshot delta;
    auto sub = [](uint64_t a, uint64_t b) { return a >= b ? a - b : 0; };
    delta.total_meta_bytes = sub(current.total_meta_bytes, baseline.total_meta_bytes);
    delta.slab_dentry_bytes = sub(current.slab_dentry_bytes, baseline.slab_dentry_bytes);
    delta.slab_inode_bytes = sub(current.slab_inode_bytes, baseline.slab_inode_bytes);
    delta.slab_ext4_inode_bytes = sub(current.slab_ext4_inode_bytes, baseline.slab_ext4_inode_bytes);
    delta.lhm_index_bytes = sub(current.lhm_index_bytes, baseline.lhm_index_bytes);
    delta.lhm_inode_bytes = sub(current.lhm_inode_bytes, baseline.lhm_inode_bytes);
    delta.lhm_string_bytes = sub(current.lhm_string_bytes, baseline.lhm_string_bytes);
    delta.process_rss_bytes = sub(current.process_rss_bytes, baseline.process_rss_bytes);
    delta.bytes_per_file = file_count == 0 ? 0 : delta.total_meta_bytes / file_count;
    return delta;
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
        } else if (arg == "--target-file-count") {
            out->target_file_count = static_cast<uint64_t>(std::stoull(require_value("--target-file-count")));
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

    if (error) {
        error->clear();
    }
    return true;
}

bool WarmAll(fsbench::IPathBackend* backend,
             const std::vector<fsbench::NamespaceEntry>& entries,
             std::string* error) {
    for (const fsbench::NamespaceEntry& entry : entries) {
        if (entry.path == "/") {
            continue;
        }
        fsbench::OpResult result;
        if (!backend->Run(entry.prepared, fsbench::OpKind::kLookupOnly, &result, error)) {
            return false;
        }
    }
    return true;
}

bool RunBackend(fsbench::IPathBackend* backend,
                const CliOptions& options,
                const fsbench::WorkloadData& workload,
                std::vector<fsbench::MemoryResultRow>* rows,
                std::string* error) {
    const uint64_t file_count = fsbench::CountFiles(workload.entries);

    fsbench::MemorySnapshot baseline_raw;
    if (!backend->SnapshotMemory(file_count, &baseline_raw, error)) {
        return false;
    }

    if (!backend->Build(workload.entries, error)) {
        return false;
    }

    fsbench::MemorySnapshot populate_raw;
    if (!backend->SnapshotMemory(file_count, &populate_raw, error)) {
        return false;
    }

    if (!WarmAll(backend, workload.entries, error)) {
        return false;
    }

    fsbench::MemorySnapshot warm_raw;
    if (!backend->SnapshotMemory(file_count, &warm_raw, error)) {
        return false;
    }

    const fsbench::MemorySnapshot baseline = DeltaSnapshot(baseline_raw, baseline_raw, file_count);
    const fsbench::MemorySnapshot populate = DeltaSnapshot(populate_raw, baseline_raw, file_count);
    const fsbench::MemorySnapshot warm = DeltaSnapshot(warm_raw, baseline_raw, file_count);

    auto append_row = [&](const char* phase, const fsbench::MemorySnapshot& snapshot) {
        fsbench::MemoryResultRow row;
        row.backend = backend->Name();
        row.file_count = file_count;
        row.depth = options.depth;
        row.siblings_per_dir = options.siblings_per_dir;
        row.files_per_leaf = options.files_per_leaf;
        row.phase = phase;
        row.snapshot = snapshot;
        rows->push_back(std::move(row));
    };

    append_row("baseline", baseline);
    append_row("populate", populate);
    append_row("warm", warm);

    std::cout << backend->Name() << " summary"
              << " file_count=" << file_count
              << " populate_bytes_per_file=" << populate.bytes_per_file
              << " warm_bytes_per_file=" << warm.bytes_per_file
              << " warm_total_meta_bytes=" << warm.total_meta_bytes
              << '\n';
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
        workload_options.target_file_count = options.target_file_count;
        workload_options.positive_queries = options.positive_queries;
        workload_options.negative_queries = options.negative_queries;
        workload_options.seed = options.seed;

        fsbench::WorkloadData workload;
        fsbench::WorkloadBuilder builder;
        if (!builder.Build(workload_options, &workload, &error)) {
            std::cerr << "failed to build workload: " << error << '\n';
            return 1;
        }

        std::vector<fsbench::MemoryResultRow> rows;
        if (options.backend == "ext4" || options.backend == "both") {
            fsbench::Ext4Backend ext4(fsbench::Ext4BackendOptions{options.mount_root, true, false});
            if (!RunBackend(&ext4, options, workload, &rows, &error)) {
                std::cerr << "ext4 benchmark failed: " << error << '\n';
                return 1;
            }
        }

        if (options.backend == "lhm" || options.backend == "both") {
            fsbench::LhmBackend lhm(fsbench::LhmBackendOptions{true});
            if (!RunBackend(&lhm, options, workload, &rows, &error)) {
                std::cerr << "lhm benchmark failed: " << error << '\n';
                return 1;
            }
        }

        if (!fsbench::WriteMemoryResults(rows, options.output_csv, &error)) {
            std::cerr << "failed to write csv: " << error << '\n';
            return 1;
        }

        std::cout << "wrote memory benchmark results to " << options.output_csv << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fatal error: " << ex.what() << '\n';
        return 1;
    }
}
