#include "nsbench/dataset_builder.h"
#include "nsbench/dataset_format.h"

#include <cerrno>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace {

const char* kDefaultSuiteDir = "VSIterate";

std::string PathJoin(const std::string& base, const std::string& leaf) {
    if (base.empty()) {
        return leaf;
    }
    if (base.back() == '/' || base.back() == '\\') {
        return base + leaf;
    }
    return base + "/" + leaf;
}

std::string DirName(const std::string& path) {
    const size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

bool MakeDirOne(const std::string& path) {
    if (path.empty()) {
        return true;
    }
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool EnsureDirectory(const std::string& path) {
    if (path.empty()) {
        return true;
    }
    std::string normalized = path;
    for (size_t i = 1; i <= normalized.size(); ++i) {
        if (i == normalized.size() || normalized[i] == '/' || normalized[i] == '\\') {
            const std::string part = normalized.substr(0, i);
            if (!part.empty() && !MakeDirOne(part)) {
                return false;
            }
        }
    }
    return true;
}

std::vector<uint32_t> ParseDepthList(const std::string& text) {
    std::vector<uint32_t> depths;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (!token.empty()) {
            depths.push_back(static_cast<uint32_t>(std::stoul(token)));
        }
    }
    return depths;
}

bool ParseArgs(int argc, char** argv, nsbench::DatasetBuildOptions* options, std::string* error) {
    if (!options) {
        if (error) {
            *error = "dataset build options output is null";
        }
        return false;
    }

    std::string suite_root;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        auto require_value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return std::string(argv[++i]);
        };

        if (arg == "--root") {
            suite_root = require_value("--root");
        } else if (arg == "--output-root") {
            options->output_root = require_value("--output-root");
        } else if (arg == "--depths") {
            options->depths = ParseDepthList(require_value("--depths"));
        } else if (arg == "--siblings-per-dir") {
            options->siblings_per_dir = static_cast<uint32_t>(std::stoul(require_value("--siblings-per-dir")));
        } else if (arg == "--files-per-leaf") {
            options->files_per_leaf = static_cast<uint32_t>(std::stoul(require_value("--files-per-leaf")));
        } else if (arg == "--positive-queries") {
            options->positive_queries_per_depth =
                static_cast<uint32_t>(std::stoul(require_value("--positive-queries")));
        } else if (arg == "--negative-queries") {
            options->negative_queries_per_depth =
                static_cast<uint32_t>(std::stoul(require_value("--negative-queries")));
        } else if (arg == "--seed") {
            options->seed = static_cast<uint32_t>(std::stoul(require_value("--seed")));
        } else if (arg == "--dataset-name") {
            options->dataset_name = require_value("--dataset-name");
        } else {
            if (error) {
                *error = "unknown argument: " + arg;
            }
            return false;
        }
    }

    if (!suite_root.empty()) {
        options->output_root = PathJoin(PathJoin(suite_root, kDefaultSuiteDir), "datasets");
    }

    if (options->output_root.empty()) {
        if (error) {
            *error = "--output-root or --root is required";
        }
        return false;
    }
    if (options->depths.empty()) {
        options->depths = {1, 2, 4, 8, 16, 32, 64};
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool SaveDatasets(const std::vector<nsbench::BuiltDataset>& datasets, std::string* error) {
    for (const nsbench::BuiltDataset& dataset : datasets) {
        const std::string dataset_dir = DirName(dataset.manifest.records_tsv);
        if (!EnsureDirectory(dataset_dir)) {
            if (error) {
                *error = "failed to create dataset directory: " + dataset_dir;
            }
            return false;
        }
        if (!nsbench::WriteNamespaceRecords(dataset.records, dataset.manifest.records_tsv, error) ||
            !nsbench::WriteQueries(dataset.positive_queries, dataset.manifest.positive_queries_tsv, error) ||
            !nsbench::WriteQueries(dataset.negative_queries, dataset.manifest.negative_queries_tsv, error)) {
            return false;
        }

        nsbench::DatasetManifest manifest = dataset.manifest;
        manifest.records_tsv = "records.tsv";
        manifest.positive_queries_tsv = "positive_queries.tsv";
        manifest.negative_queries_tsv = "negative_queries.tsv";
        if (!nsbench::WriteManifest(manifest, PathJoin(dataset_dir, "manifest.txt"), error)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        nsbench::DatasetBuildOptions options;
        std::string error;
        if (!ParseArgs(argc, argv, &options, &error)) {
            std::cerr << "nsbench_build_dataset: " << error << '\n';
            return 1;
        }

        nsbench::DatasetBuilder builder;
        std::vector<nsbench::BuiltDataset> datasets;
        if (!builder.BuildAll(options, &datasets, &error) || !SaveDatasets(datasets, &error)) {
            std::cerr << "nsbench_build_dataset: " << error << '\n';
            return 1;
        }

        std::cout << "built " << datasets.size() << " dataset(s)\n";
        for (const nsbench::BuiltDataset& dataset : datasets) {
            std::cout << "  depth=" << dataset.manifest.depth
                      << " records=" << dataset.records.size()
                      << " queries=" << dataset.manifest.total_queries
                      << " manifest=" << PathJoin(DirName(dataset.manifest.records_tsv), "manifest.txt")
                      << '\n';
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "nsbench_build_dataset error: " << ex.what() << '\n';
        return 1;
    }
}
