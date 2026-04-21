#pragma once

#include "fsbench/common_types.h"

#include <string>
#include <vector>

namespace fsbench {

class IPathBackend {
public:
    virtual ~IPathBackend() = default;

    virtual const char* Name() const noexcept = 0;

    virtual bool Build(const std::vector<NamespaceEntry>& entries,
                       std::string* error) = 0;

    virtual bool Run(const PreparedPath& path,
                     OpKind op,
                     OpResult* out,
                     std::string* error) = 0;

    virtual bool SnapshotMemory(uint64_t file_count,
                                MemorySnapshot* out,
                                std::string* error) const = 0;
};

}  // namespace fsbench
