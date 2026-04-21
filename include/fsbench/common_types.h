#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fsbench {

enum class NodeType : uint8_t {
    kDirectory = 1,
    kFile = 2,
};

enum class OpKind : uint8_t {
    kLookupOnly = 1,
    kOpenRead4K = 2,
    kOpenWrite4K = 3,
    kNegativeLookup = 4,
};

struct PreparedPath {
    std::string normalized;
    std::vector<std::string> components;

    uint32_t depth() const noexcept {
        return static_cast<uint32_t>(components.size());
    }
};

struct NamespaceEntry {
    uint64_t inode_id = 0;
    uint64_t parent_inode_id = 0;
    NodeType type = NodeType::kFile;
    std::string path;
    PreparedPath prepared;
};

struct Query {
    PreparedPath prepared;
    bool expect_found = true;
};

struct OpResult {
    bool ok = false;
    uint64_t latency_ns = 0;
    uint64_t bytes = 0;
    uint32_t depth = 0;
};

struct MemorySnapshot {
    uint64_t total_meta_bytes = 0;
    uint64_t bytes_per_file = 0;

    uint64_t slab_dentry_bytes = 0;
    uint64_t slab_inode_bytes = 0;
    uint64_t slab_ext4_inode_bytes = 0;

    uint64_t lhm_index_bytes = 0;
    uint64_t lhm_inode_bytes = 0;
    uint64_t lhm_string_bytes = 0;
    uint64_t process_rss_bytes = 0;
};

}  // namespace fsbench
