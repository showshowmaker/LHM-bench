#include "flatbench/flatlike_backend.h"
#include "flatbench/stats.h"
#include "flatbench/workload.h"
#include "nsbench/path_utils.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <thread>
#endif

namespace {

struct CliOptions {
    size_t subtree_files{50000};
    size_t rename_iterations{20};
    size_t lookup_threads{4};
    size_t lookup_per_thread{5000};
    size_t leaf_capacity{128};
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
        if (arg == "--subtree-files") {
            options->subtree_files = static_cast<size_t>(std::stoul(require_value("--subtree-files")));
        } else if (arg == "--rename-iterations") {
            options->rename_iterations = static_cast<size_t>(std::stoul(require_value("--rename-iterations")));
        } else if (arg == "--lookup-threads") {
            options->lookup_threads = static_cast<size_t>(std::stoul(require_value("--lookup-threads")));
        } else if (arg == "--lookup-per-thread") {
            options->lookup_per_thread = static_cast<size_t>(std::stoul(require_value("--lookup-per-thread")));
        } else if (arg == "--leaf-capacity") {
            options->leaf_capacity = static_cast<size_t>(std::stoul(require_value("--leaf-capacity")));
        } else {
            if (error) {
                *error = "unknown argument: " + arg;
            }
            return false;
        }
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool RewritePrefix(const nsbench::PreparedPath& original,
                   const nsbench::PreparedPath& from,
                   const nsbench::PreparedPath& to,
                   nsbench::PreparedPath* out,
                   std::string* error) {
    if (!out) {
        if (error) {
            *error = "rewrite output is null";
        }
        return false;
    }
    if (original.normalized.compare(0, from.normalized.size(), from.normalized) != 0) {
        if (error) {
            *error = "lookup path does not share rename prefix";
        }
        return false;
    }
    return nsbench::PreparePath(to.normalized + original.normalized.substr(from.normalized.size()), out, error);
}

struct LookupWorkerContext {
    flatbench::FlatLikeBackend* backend;
    const std::vector<nsbench::PreparedPath>* old_paths;
    const std::vector<nsbench::PreparedPath>* new_paths;
    std::atomic<int>* active_prefix;
    std::atomic<bool>* start;
    size_t lookup_per_thread;
    unsigned int seed;
    std::vector<uint64_t>* latencies;
};

void RunLookupWorker(LookupWorkerContext* ctx) {
    std::mt19937 rng(ctx->seed);
    while (!ctx->start->load()) {
    }
    for (size_t i = 0; i < ctx->lookup_per_thread; ++i) {
        const std::vector<nsbench::PreparedPath>& paths =
            (ctx->active_prefix->load() == 0) ? *ctx->old_paths : *ctx->new_paths;
        const nsbench::PreparedPath& path = paths[rng() % paths.size()];
        flatbench::OpStats stats;
        std::string worker_error;
        const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        if (!ctx->backend->Lookup(path, &stats, &worker_error)) {
            continue;
        }
        const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        ctx->latencies->push_back(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()));
    }
}

#ifdef _WIN32
unsigned __stdcall LookupWorkerMain(void* arg) {
    RunLookupWorker(static_cast<LookupWorkerContext*>(arg));
    return 0;
}
#endif

}  // namespace

int main(int argc, char** argv) {
    try {
        CliOptions options;
        std::string error;
        if (!ParseArgs(argc, argv, &options, &error)) {
            std::cerr << "flatbench_rename: " << error << '\n';
            return 1;
        }

        std::vector<nsbench::NamespaceRecord> records;
        nsbench::PreparedPath src_prefix;
        nsbench::PreparedPath dst_prefix;
        std::vector<nsbench::PreparedPath> old_lookup_paths;
        if (!flatbench::BuildRenameWorkload(options.subtree_files,
                                            &records,
                                            &src_prefix,
                                            &dst_prefix,
                                            &old_lookup_paths,
                                            &error)) {
            std::cerr << "flatbench_rename: " << error << '\n';
            return 1;
        }

        std::vector<nsbench::PreparedPath> new_lookup_paths;
        new_lookup_paths.reserve(old_lookup_paths.size());
        for (size_t i = 0; i < old_lookup_paths.size(); ++i) {
            nsbench::PreparedPath rewritten;
            if (!RewritePrefix(old_lookup_paths[i], src_prefix, dst_prefix, &rewritten, &error)) {
                std::cerr << "flatbench_rename: " << error << '\n';
                return 1;
            }
            new_lookup_paths.push_back(rewritten);
        }

        flatbench::FlatLikeOptions backend_options;
        backend_options.leaf_capacity = options.leaf_capacity;
        flatbench::FlatLikeBackend backend(backend_options);
        if (!backend.Build(records, &error)) {
            std::cerr << "flatbench_rename: " << error << '\n';
            return 1;
        }

        backend.ResetCounters();
        std::vector<uint64_t> rename_latencies;
        rename_latencies.reserve(options.rename_iterations);
        std::vector<std::vector<uint64_t> > lookup_latencies(options.lookup_threads);
        std::atomic<int> active_prefix(0);
        std::atomic<bool> start(false);

        std::vector<LookupWorkerContext> worker_contexts(options.lookup_threads);
#ifdef _WIN32
        std::vector<HANDLE> workers;
#else
        std::vector<std::thread> workers;
#endif
        for (size_t tid = 0; tid < options.lookup_threads; ++tid) {
            worker_contexts[tid].backend = &backend;
            worker_contexts[tid].old_paths = &old_lookup_paths;
            worker_contexts[tid].new_paths = &new_lookup_paths;
            worker_contexts[tid].active_prefix = &active_prefix;
            worker_contexts[tid].start = &start;
            worker_contexts[tid].lookup_per_thread = options.lookup_per_thread;
            worker_contexts[tid].seed = static_cast<unsigned int>(tid + 17);
            worker_contexts[tid].latencies = &lookup_latencies[tid];
#ifdef _WIN32
            uintptr_t handle =
                _beginthreadex(NULL, 0, &LookupWorkerMain, &worker_contexts[tid], 0, NULL);
            if (handle == 0) {
                std::cerr << "flatbench_rename: failed to spawn worker thread\n";
                return 1;
            }
            workers.push_back(reinterpret_cast<HANDLE>(handle));
#else
            workers.push_back(std::thread([&, tid]() { RunLookupWorker(&worker_contexts[tid]); }));
#endif
        }

        start.store(true);
        nsbench::PreparedPath from = src_prefix;
        nsbench::PreparedPath to = dst_prefix;
        for (size_t i = 0; i < options.rename_iterations; ++i) {
            flatbench::OpStats rename_stats;
            const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            if (!backend.RenameSubtree(from, to, &rename_stats, &error)) {
                std::cerr << "flatbench_rename: " << error << '\n';
                return 1;
            }
            const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            rename_latencies.push_back(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()));
            active_prefix.store(active_prefix.load() == 0 ? 1 : 0);
            std::swap(from, to);
        }

        for (size_t i = 0; i < workers.size(); ++i) {
#ifdef _WIN32
            WaitForSingleObject(workers[i], INFINITE);
            CloseHandle(workers[i]);
#else
            workers[i].join();
#endif
        }

        std::vector<uint64_t> all_lookup_latencies;
        for (size_t i = 0; i < lookup_latencies.size(); ++i) {
            all_lookup_latencies.insert(all_lookup_latencies.end(),
                                        lookup_latencies[i].begin(),
                                        lookup_latencies[i].end());
        }

        const flatbench::LatencySummary rename_summary = flatbench::SummarizeLatencies(rename_latencies);
        const flatbench::LatencySummary lookup_summary = flatbench::SummarizeLatencies(all_lookup_latencies);
        const flatbench::FlatInternalCounters counters = backend.Counters();

        std::cout << "subtree_files,rename_iterations,lookup_threads,lookup_ops,rename_avg_ns,rename_p95_ns,"
                     "rename_p99_ns,lookup_avg_ns,lookup_p95_ns,lookup_p99_ns,rename_key_updates,"
                     "tree_write_lock_ns,reader_blocked_ns\n";
        std::cout << options.subtree_files << ','
                  << options.rename_iterations << ','
                  << options.lookup_threads << ','
                  << all_lookup_latencies.size() << ','
                  << rename_summary.avg_ns << ','
                  << rename_summary.p95_ns << ','
                  << rename_summary.p99_ns << ','
                  << lookup_summary.avg_ns << ','
                  << lookup_summary.p95_ns << ','
                  << lookup_summary.p99_ns << ','
                  << counters.rename_key_updates << ','
                  << counters.tree_write_lock_ns << ','
                  << counters.reader_blocked_ns << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "flatbench_rename error: " << ex.what() << '\n';
        return 1;
    }
}
