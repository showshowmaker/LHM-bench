#pragma once

#include "nsbench/common_types.h"

#include <cstdint>
#include <string>

namespace nsbench {

struct RocksDentryValue {
    uint64_t child_inode_id{0};
    NodeType child_type{NodeType::kFile};
};

std::string EncodeInodeKey(uint64_t inode_id);
std::string EncodeDentryKey(uint64_t parent_inode_id, const std::string& name);

std::string EncodeInodeValue(const NamespaceRecord& record);
bool DecodeInodeValue(const std::string& value, NamespaceRecord* record);

std::string EncodeDentryValue(uint64_t child_inode_id, NodeType child_type);
bool DecodeDentryValue(const std::string& value, RocksDentryValue* out);

}  // namespace nsbench
