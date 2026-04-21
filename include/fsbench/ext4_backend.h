#pragma once

#include "fsbench/backend.h"

#include <string>

namespace fsbench {

struct Ext4BackendOptions {
    std::string mount_root;
    bool create_missing_dirs = true;
    bool fsync_writes = false;
};

class Ext4Backend final : public IPathBackend {
public:
    explicit Ext4Backend(Ext4BackendOptions options);

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
    bool BuildOne(const NamespaceEntry& entry, std::string* error);
    bool LookupOnly(const PreparedPath& path, OpResult* out, std::string* error);
    bool OpenRead4K(const PreparedPath& path, OpResult* out, std::string* error);
    bool OpenWrite4K(const PreparedPath& path, OpResult* out, std::string* error);
    std::string ToHostPath(const PreparedPath& path) const;

private:
    Ext4BackendOptions options_;
};

}  // namespace fsbench
