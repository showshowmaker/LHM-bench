#pragma once

#include "nsbench/common_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace flatbench {

static const uint64_t kFlatNodeHeaderBytes = 32;
static const uint64_t kFlatEntryHeaderBytes = 24;

struct FlatEntry {
    std::string full_key;
    uint64_t inode_id{0};
    nsbench::NodeType type{nsbench::NodeType::kFile};
};

struct FlatNode {
    std::string common_prefix;
    std::vector<FlatEntry> entries;
};

}  // namespace flatbench
