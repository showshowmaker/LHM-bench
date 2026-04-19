#include "nsbench/bench_runner.h"
#include "nsbench/csv_writer.h"
#include "nsbench/dataset_format.h"
#include "nsbench/masstree_resolver.h"
#include "nsbench/resolver.h"

#if NSBENCH_HAVE_ROCKSDB
#include "nsbench/rocks_iterative_resolver.h"
#endif

#include <cerrno>
#include <fstream>
#include <iostream>
#include <memory>
#include <algorithm>
#include <stdexcept>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace {

const char* kDefaultSuiteDir = "VSIterate";

struct CliOptions {
    std::string backend{"masstree"};
    std::vector<std::string> manifest_paths;
    std::vector<std::string> manifest_list_paths;
    std::string artifact_root;
    std::string db_path;
    uint32_t warmup_queries{10000};
    uint32_t repeats{5};
    bool include_negative{false};
    bool verify{true};
    std::string output_csv;
};

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

bool IsAbsolutePath(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return true;
    }
    return path.size() > 1 && path[1] == ':';
}

std::string BuildArtifactDir(const CliOptions& options) {
    if (options.artifact_root.empty()) {
        return "";
    }
    return PathJoin(options.artifact_root, kDefaultSuiteDir);
}

bool ReadManifestListFile(const std::string& list_path,
                          std::vector<std::string>* manifest_paths,
                          std::string* error) {
    if (!manifest_paths) {
        if (error) {
            *error = "manifest paths output is null";
        }
        return false;
    }
    std::ifstream in(list_path.c_str());
    if (!in) {
        if (error) {
            *error = "failed to open manifest list: " + list_path;
        }
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        const size_t begin = line.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos || line[begin] == '#') {
            continue;
        }
        const size_t end = line.find_last_not_of(" \t\r\n");
        manifest_paths->push_back(line.substr(begin, end - begin + 1));
    }
    return true;
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

        if (arg == "--backend") {
            options->backend = require_value("--backend");
        } else if (arg == "--manifest") {
            options->manifest_paths.push_back(require_value("--manifest"));
        } else if (arg == "--manifest-list") {
            options->manifest_list_paths.push_back(require_value("--manifest-list"));
        } else if (arg == "--artifact-root") {
            options->artifact_root = require_value("--artifact-root");
        } else if (arg == "--db-path") {
            options->db_path = require_value("--db-path");
        } else if (arg == "--warmup") {
            options->warmup_queries = static_cast<uint32_t>(std::stoul(require_value("--warmup")));
        } else if (arg == "--repeats") {
            options->repeats = static_cast<uint32_t>(std::stoul(require_value("--repeats")));
        } else if (arg == "--output-csv") {
            options->output_csv = require_value("--output-csv");
        } else if (arg == "--include-negative") {
            options->include_negative = true;
        } else if (arg == "--no-verify") {
            options->verify = false;
        } else {
            if (error) {
                *error = "unknown argument: " + arg;
            }
            return false;
        }
    }

    for (size_t i = 0; i < options->manifest_list_paths.size(); ++i) {
        if (!ReadManifestListFile(options->manifest_list_paths[i], &options->manifest_paths, error)) {
            return false;
        }
    }

    if (options->manifest_paths.empty()) {
        if (error) {
            *error = "at least one --manifest or --manifest-list is required";
        }
        return false;
    }

    std::sort(options->manifest_paths.begin(), options->manifest_paths.end());
    options->manifest_paths.erase(std::unique(options->manifest_paths.begin(), options->manifest_paths.end()),
                                  options->manifest_paths.end());

    if (options->output_csv.empty() && !options->artifact_root.empty()) {
        options->output_csv =
            PathJoin(BuildArtifactDir(*options), options->backend + "_depth_sweep.csv");
    } else if (options->output_csv.empty()) {
        options->output_csv = PathJoin(DirName(options->manifest_paths.front()),
                                       options->backend + "_results.csv");
    }
    if (error) {
        error->clear();
    }
    return true;
}

std::string ResolvePathFromManifest(const std::string& manifest_path,
                                    const std::string& candidate) {
    if (IsAbsolutePath(candidate)) {
        return candidate;
    }
    return PathJoin(DirName(manifest_path), candidate);
}

struct LoadedManifestData {
    std::string manifest_path;
    nsbench::DatasetManifest manifest;
    std::vector<nsbench::NamespaceRecord> records;
    std::vector<nsbench::QueryRecord> positive_queries;
    std::vector<nsbench::QueryRecord> negative_queries;
};

std::unique_ptr<nsbench::IPathResolver> CreateResolver(const CliOptions& options,
                                                       const std::string& manifest_path,
                                                       std::string* error) {
    if (options.backend == "masstree") {
        return std::unique_ptr<nsbench::IPathResolver>(new nsbench::MasstreeResolver());
    }
#if NSBENCH_HAVE_ROCKSDB
    if (options.backend == "rocksdb") {
        nsbench::RocksResolverOptions rocks_options;
        if (!options.db_path.empty()) {
            rocks_options.db_path = options.db_path;
        } else if (!options.artifact_root.empty()) {
            rocks_options.db_path = PathJoin(BuildArtifactDir(options), "rocksdb_nsbench");
        } else {
            rocks_options.db_path = PathJoin(DirName(manifest_path), "rocksdb_nsbench");
        }
        return std::unique_ptr<nsbench::IPathResolver>(
            new nsbench::RocksIterativeResolver(std::move(rocks_options)));
    }
#endif
    if (error) {
        *error = "unsupported backend: " + options.backend;
    }
    return std::unique_ptr<nsbench::IPathResolver>();
}

bool LoadManifestData(const std::string& manifest_path,
                      bool include_negative,
                      LoadedManifestData* out,
                      std::string* error) {
    if (!out) {
        if (error) {
            *error = "loaded manifest output is null";
        }
        return false;
    }

    LoadedManifestData loaded;
    loaded.manifest_path = manifest_path;
    if (!nsbench::ReadManifest(manifest_path, &loaded.manifest, error)) {
        return false;
    }
    if (!nsbench::ReadNamespaceRecords(
            ResolvePathFromManifest(manifest_path, loaded.manifest.records_tsv),
            &loaded.records,
            error) ||
        !nsbench::ReadQueries(
            ResolvePathFromManifest(manifest_path, loaded.manifest.positive_queries_tsv),
            &loaded.positive_queries,
            error)) {
        return false;
    }
    if (include_negative &&
        !nsbench::ReadQueries(
            ResolvePathFromManifest(manifest_path, loaded.manifest.negative_queries_tsv),
            &loaded.negative_queries,
            error)) {
        return false;
    }
    *out = std::move(loaded);
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
            std::cerr << "nsbench_run: " << error << '\n';
            return 1;
        }

        if (!options.artifact_root.empty() && !EnsureDirectory(BuildArtifactDir(options))) {
            std::cerr << "nsbench_run: failed to create artifact root directory\n";
            return 1;
        }

        nsbench::BenchRunner runner;
        nsbench::RunOptions run_options;
        run_options.warmup_queries = options.warmup_queries;
        run_options.repeats = options.repeats;
        run_options.check_correctness = options.verify;

        std::vector<LoadedManifestData> loaded_manifests;
        loaded_manifests.reserve(options.manifest_paths.size());
        for (size_t i = 0; i < options.manifest_paths.size(); ++i) {
            LoadedManifestData loaded;
            if (!LoadManifestData(options.manifest_paths[i], options.include_negative, &loaded, &error)) {
                std::cerr << "nsbench_run: " << error << '\n';
                return 1;
            }
            loaded_manifests.push_back(std::move(loaded));
        }
        std::sort(loaded_manifests.begin(), loaded_manifests.end(),
                  [](const LoadedManifestData& lhs, const LoadedManifestData& rhs) {
                      if (lhs.manifest.depth != rhs.manifest.depth) {
                          return lhs.manifest.depth < rhs.manifest.depth;
                      }
                      return lhs.manifest_path < rhs.manifest_path;
                  });

        std::vector<nsbench::BenchmarkReport> reports;
        for (size_t i = 0; i < loaded_manifests.size(); ++i) {
            const LoadedManifestData& loaded = loaded_manifests[i];
            std::unique_ptr<nsbench::IPathResolver> resolver =
                CreateResolver(options, loaded.manifest_path, &error);
            if (!resolver.get()) {
                std::cerr << "nsbench_run: " << error << '\n';
                return 1;
            }
            if (!resolver->Build(loaded.records, &error)) {
                std::cerr << "nsbench_run: " << error << '\n';
                return 1;
            }

            nsbench::BenchmarkReport positive_report;
            if (!runner.Run(resolver.get(),
                            loaded.manifest.depth,
                            std::string("positive"),
                            loaded.positive_queries,
                            run_options,
                            &positive_report,
                            &error)) {
                std::cerr << "nsbench_run: " << error << '\n';
                return 1;
            }
            positive_report.dataset_name = loaded.manifest.dataset_name;
            positive_report.manifest_path = loaded.manifest_path;
            reports.push_back(positive_report);

            if (options.include_negative && !loaded.negative_queries.empty()) {
                nsbench::BenchmarkReport negative_report;
                if (!runner.Run(resolver.get(),
                                loaded.manifest.depth,
                                std::string("negative"),
                                loaded.negative_queries,
                                run_options,
                                &negative_report,
                                &error)) {
                    std::cerr << "nsbench_run: " << error << '\n';
                    return 1;
                }
                negative_report.dataset_name = loaded.manifest.dataset_name;
                negative_report.manifest_path = loaded.manifest_path;
                reports.push_back(negative_report);
            }
        }

        if (!EnsureDirectory(DirName(options.output_csv))) {
            std::cerr << "nsbench_run: failed to create output directory\n";
            return 1;
        }
        if (!nsbench::WriteBenchmarkReportsCsv(reports, options.output_csv, &error)) {
            std::cerr << "nsbench_run: " << error << '\n';
            return 1;
        }

        std::cout << "backend=" << options.backend
                  << " manifests=" << loaded_manifests.size()
                  << " csv=" << options.output_csv
                  << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "nsbench_run error: " << ex.what() << '\n';
        return 1;
    }
}
