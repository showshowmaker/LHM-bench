#include "flatbench/flatlike_backend.h"
#include "flatbench/workload.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct CliOptions {
    std::vector<uint32_t> depths;
    uint32_t siblings_per_dir{8};
    uint32_t files_per_leaf{32};
    size_t leaf_capacity{64};
    std::string output_csv;
};

std::vector<uint32_t> ParseDepths(const std::string& csv) {
    std::vector<uint32_t> depths;
    std::stringstream stream(csv);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (!token.empty()) {
            depths.push_back(static_cast<uint32_t>(std::stoul(token)));
        }
    }
    return depths;
}

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
        if (arg == "--depths") {
            options->depths = ParseDepths(require_value("--depths"));
        } else if (arg == "--siblings-per-dir") {
            options->siblings_per_dir = static_cast<uint32_t>(std::stoul(require_value("--siblings-per-dir")));
        } else if (arg == "--files-per-leaf") {
            options->files_per_leaf = static_cast<uint32_t>(std::stoul(require_value("--files-per-leaf")));
        } else if (arg == "--leaf-capacity") {
            options->leaf_capacity = static_cast<size_t>(std::stoul(require_value("--leaf-capacity")));
        } else if (arg == "--output-csv") {
            options->output_csv = require_value("--output-csv");
        } else {
            if (error) {
                *error = "unknown argument: " + arg;
            }
            return false;
        }
    }

    if (options->depths.empty()) {
        options->depths.push_back(4);
        options->depths.push_back(8);
        options->depths.push_back(16);
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool WriteCsv(const std::string& path, const std::string& data, std::string* error) {
    if (path.empty()) {
        return true;
    }
    std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
    if (!out) {
        if (error) {
            *error = "failed to open output csv: " + path;
        }
        return false;
    }
    out << data;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        CliOptions options;
        std::string error;
        if (!ParseArgs(argc, argv, &options, &error)) {
            std::cerr << "flatbench_prefix: " << error << '\n';
            return 1;
        }

        std::ostringstream csv;
        csv << "depth,entries,node_count,leaf_capacity,logical_path_bytes,total_bytes,unique_trie_bytes,"
               "duplicated_prefix_bytes,node_prefix_bytes,node_suffix_bytes,metadata_overhead_bytes\n";

        for (size_t i = 0; i < options.depths.size(); ++i) {
            std::vector<nsbench::NamespaceRecord> records;
            if (!flatbench::BuildPrefixRecords(options.depths[i],
                                               options.siblings_per_dir,
                                               options.files_per_leaf,
                                               &records,
                                               &error)) {
                std::cerr << "flatbench_prefix: " << error << '\n';
                return 1;
            }

            flatbench::FlatLikeOptions backend_options;
            backend_options.leaf_capacity = options.leaf_capacity;
            flatbench::FlatLikeBackend backend(backend_options);
            if (!backend.Build(records, &error)) {
                std::cerr << "flatbench_prefix: " << error << '\n';
                return 1;
            }

            const flatbench::MemoryStats stats = backend.SnapshotMemory();
            csv << options.depths[i] << ','
                << stats.entry_count << ','
                << stats.node_count << ','
                << options.leaf_capacity << ','
                << stats.logical_path_bytes << ','
                << stats.total_bytes << ','
                << stats.unique_trie_bytes << ','
                << stats.duplicated_prefix_bytes << ','
                << stats.node_prefix_bytes << ','
                << stats.node_suffix_bytes << ','
                << stats.metadata_overhead_bytes << '\n';
        }

        std::cout << csv.str();
        if (!WriteCsv(options.output_csv, csv.str(), &error)) {
            std::cerr << "flatbench_prefix: " << error << '\n';
            return 1;
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "flatbench_prefix error: " << ex.what() << '\n';
        return 1;
    }
}
