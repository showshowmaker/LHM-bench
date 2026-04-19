#pragma once

#include "flatbench/stats.h"
#include "nsbench/common_types.h"

#include <string>
#include <vector>

namespace flatbench {

class INamespaceBackend {
public:
    virtual ~INamespaceBackend() = default;

    virtual const char* Name() const noexcept = 0;

    virtual bool Build(const std::vector<nsbench::NamespaceRecord>& records,
                       std::string* error) = 0;

    virtual bool Lookup(const nsbench::PreparedPath& path,
                        OpStats* out,
                        std::string* error) const = 0;

    virtual bool Insert(const nsbench::NamespaceRecord& record,
                        OpStats* out,
                        std::string* error) = 0;

    virtual bool Erase(const nsbench::PreparedPath& path,
                       OpStats* out,
                       std::string* error) = 0;

    virtual bool RenameSubtree(const nsbench::PreparedPath& src,
                               const nsbench::PreparedPath& dst,
                               OpStats* out,
                               std::string* error) = 0;

    virtual MemoryStats SnapshotMemory() const = 0;
    virtual FlatInternalCounters Counters() const = 0;
    virtual void ResetCounters() = 0;
};

}  // namespace flatbench
