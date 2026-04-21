#include "fsbench/lhm_backend.h"

#include "fsbench/proc_mem_sampler.h"
#include "nsbench/masstree_runtime.h"

#include <chrono>

namespace fsbench {
namespace {

uint64_t NowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace

LhmBackend::LhmBackend(LhmBackendOptions options)
    : options_(std::move(options)) {
}

LhmBackend::~LhmBackend() {
    if (ti_) {
        ns_.destroy(*ti_);
        nsbench::DestroyThreadInfo(ti_);
        ti_ = nullptr;
    }
}

const char* LhmBackend::Name() const noexcept {
    return "lhm";
}

bool LhmBackend::Build(const std::vector<NamespaceEntry>& entries,
                       std::string* error) {
    if (!ti_) {
        ti_ = nsbench::CreateMainThreadInfo();
        ns_.initialize(*ti_);
    }

    counters_ = MemoryCounters{};
    for (const NamespaceEntry& entry : entries) {
        if (entry.path == "/") {
            continue;
        }

        bool ok = false;
        if (entry.type == NodeType::kDirectory) {
            ok = ns_.mkdir(entry.path, entry.inode_id, *ti_);
        } else {
            ok = ns_.creat_file(entry.path, entry.inode_id, *ti_);
        }
        if (!ok) {
            if (error) {
                *error = "failed to insert into LHM namespace: " + entry.path;
            }
            return false;
        }
        if (options_.track_memory) {
            AccountEntry(entry);
        }
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool LhmBackend::Run(const PreparedPath& path,
                     OpKind op,
                     OpResult* out,
                     std::string* error) {
    switch (op) {
    case OpKind::kLookupOnly:
    case OpKind::kNegativeLookup:
        return LookupOnly(path, out, error);
    case OpKind::kOpenRead4K:
    case OpKind::kOpenWrite4K:
        return LookupOnly(path, out, error);
    default:
        if (error) {
            *error = "unsupported LHM op";
        }
        return false;
    }
}

bool LhmBackend::SnapshotMemory(uint64_t file_count,
                                MemorySnapshot* out,
                                std::string* error) const {
    if (!out) {
        if (error) {
            *error = "memory snapshot output is null";
        }
        return false;
    }

    MemorySnapshot snapshot;
    snapshot.lhm_index_bytes = counters_.index_bytes;
    snapshot.lhm_inode_bytes = counters_.inode_bytes;
    snapshot.lhm_string_bytes = counters_.string_bytes;
    snapshot.total_meta_bytes = counters_.total();
    snapshot.bytes_per_file = file_count == 0 ? 0 : snapshot.total_meta_bytes / file_count;

    ProcMemInfo mem_info;
    ProcMemSampler sampler;
    if (sampler.SnapshotSelf(&mem_info, nullptr)) {
        snapshot.process_rss_bytes = mem_info.rss_bytes;
    }

    *out = snapshot;
    if (error) {
        error->clear();
    }
    return true;
}

bool LhmBackend::LookupOnly(const PreparedPath& path,
                            OpResult* out,
                            std::string* error) {
    if (!ti_) {
        if (error) {
            *error = "LHM backend is not initialized";
        }
        return false;
    }

    uint64_t inode = 0;
    const uint64_t begin = NowNs();
    const bool found = ns_.lookup_inode(path.normalized, inode, *ti_);
    const uint64_t end = NowNs();

    if (out) {
        out->ok = found;
        out->latency_ns = end - begin;
        out->bytes = 0;
        out->depth = path.depth();
    }
    if (error) {
        error->clear();
    }
    return true;
}

void LhmBackend::AccountEntry(const NamespaceEntry& entry) {
    counters_.inode_bytes += sizeof(MasstreeLHM::namespace_entry);
    counters_.index_bytes += entry.prepared.depth() * sizeof(uint64_t);
    if (!entry.prepared.components.empty()) {
        counters_.string_bytes += entry.prepared.components.back().size() + 1;
    }
    if (entry.type == NodeType::kDirectory) {
        counters_.index_bytes += 32;
    }
}

}  // namespace fsbench
