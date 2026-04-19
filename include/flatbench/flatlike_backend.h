#pragma once

#include "flatbench/backend.h"
#include "flatbench/flat_tree.h"

#include <cstddef>

namespace flatbench {

struct FlatLikeOptions {
    size_t leaf_capacity{64};
};

class FlatLikeBackend : public INamespaceBackend {
public:
    explicit FlatLikeBackend(const FlatLikeOptions& options);

    const char* Name() const noexcept override;
    bool Build(const std::vector<nsbench::NamespaceRecord>& records,
               std::string* error) override;
    bool Lookup(const nsbench::PreparedPath& path,
                OpStats* out,
                std::string* error) const override;
    bool Insert(const nsbench::NamespaceRecord& record,
                OpStats* out,
                std::string* error) override;
    bool Erase(const nsbench::PreparedPath& path,
               OpStats* out,
               std::string* error) override;
    bool RenameSubtree(const nsbench::PreparedPath& src,
                       const nsbench::PreparedPath& dst,
                       OpStats* out,
                       std::string* error) override;
    MemoryStats SnapshotMemory() const override;
    FlatInternalCounters Counters() const override;
    void ResetCounters() override;

    size_t leaf_count() const;
    size_t entry_count() const;

private:
    FlatTree tree_;
};

}  // namespace flatbench
