#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nsbench {

enum class NodeType : uint8_t {
    kDirectory = 1,
    kFile = 2,
};

struct PreparedPath {
    std::string normalized;
    std::vector<std::string> components;
    std::vector<uint64_t> masstree_hashes;

    uint32_t depth() const noexcept {
        return static_cast<uint32_t>(components.size());
    }
};

struct NamespaceRecord {
    uint64_t inode_id{0};
    uint64_t parent_inode_id{0};
    NodeType type{NodeType::kFile};
    std::string path;
    PreparedPath prepared;
};

struct QueryRecord {
    std::string path;
    PreparedPath prepared;
    bool expect_found{true};
    uint64_t expected_inode_id{0};
    uint32_t expected_depth{0};
};

inline const char* NodeTypeName(NodeType type) noexcept {
    switch (type) {
    case NodeType::kDirectory:
        return "dir";
    case NodeType::kFile:
        return "file";
    default:
        return "unknown";
    }
}

}  // namespace nsbench
