#pragma once

#include "fsbench/backend.h"

#include "lhm_namespace.hh"

struct threadinfo;

namespace fsbench {

struct LhmBackendOptions {
    bool track_memory = true;
};

class LhmBackend final : public IPathBackend {
public:
    explicit LhmBackend(LhmBackendOptions options);
    ~LhmBackend() override;

    const char* Name() const noexcept override;

    bool Build(const std::vector<NamespaceEntry>& entries,
               std::string* error) override;

    bool Run(const PreparedPath& path,
             OpKind op,
             OpResult* out,
             std::string* error) override;

    bool SnapshotMemory(uint64_t file_count,
                        MemorySnapshot* out,
                        std::string* error) const override;

private:
    struct MemoryCounters {
        uint64_t index_bytes = 0;
        uint64_t inode_bytes = 0;
        uint64_t string_bytes = 0;

        uint64_t total() const noexcept {
            return index_bytes + inode_bytes + string_bytes;
        }
    };

    bool LookupOnly(const PreparedPath& path, OpResult* out, std::string* error);
    void AccountEntry(const NamespaceEntry& entry);

private:
    LhmBackendOptions options_;
    threadinfo* ti_ = nullptr;
    mutable MasstreeLHM::LhmNamespace ns_;
    MemoryCounters counters_;
};

}  // namespace fsbench
