#pragma once

#include "fsbench/common_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fsbench {

struct WorkloadOptions {
    uint32_t depth = 8;
    uint32_t siblings_per_dir = 16;
    uint32_t files_per_leaf = 64;
    uint32_t positive_queries = 10000;
    uint32_t negative_queries = 10000;
    uint32_t seed = 1;
};

struct WorkloadData {
    std::vector<NamespaceEntry> entries;
    std::vector<Query> positive_queries;
    std::vector<Query> negative_queries;
};

class WorkloadBuilder {
public:
    bool Build(const WorkloadOptions& options,
               WorkloadData* out,
               std::string* error) const;
};

uint64_t CountFiles(const std::vector<NamespaceEntry>& entries);

}  // namespace fsbench
