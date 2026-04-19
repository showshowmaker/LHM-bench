#pragma once

#include "nsbench/common_types.h"

#include <string>
#include <vector>

namespace nsbench {

struct ResolveResult {
    bool found{false};
    uint64_t inode_id{0};
    uint32_t depth{0};
    uint32_t component_steps{0};
    uint32_t index_steps{0};
    uint32_t steps{0};
};

class IPathResolver {
public:
    virtual ~IPathResolver() = default;

    virtual const char* Name() const noexcept = 0;

    virtual bool Build(const std::vector<NamespaceRecord>& records,
                       std::string* error) = 0;

    virtual bool Resolve(const PreparedPath& path,
                         ResolveResult* out,
                         std::string* error) const = 0;

    virtual bool Warmup(const std::vector<QueryRecord>& queries,
                        std::string* error) const = 0;
};

}  // namespace nsbench
