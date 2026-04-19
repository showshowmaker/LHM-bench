#pragma once

#include <cstdint>
#include <vector>

namespace flatbench {

struct OpStats {
    bool ok{false};
    uint64_t latency_ns{0};
    uint64_t bytes_moved{0};
    uint64_t bytes_rewritten{0};
    uint64_t nodes_touched{0};
    uint64_t lock_wait_ns{0};
    uint64_t lock_hold_ns{0};
};

struct FlatInternalCounters {
    uint64_t node_prefix_recompute{0};
    uint64_t prefix_change_count{0};
    uint64_t prefix_expand_bytes{0};
    uint64_t prefix_shrink_bytes{0};
    uint64_t suffix_expand_bytes{0};
    uint64_t suffix_shrink_bytes{0};
    uint64_t entry_reencoded{0};
    uint64_t node_split{0};
    uint64_t node_merge{0};
    uint64_t subtree_slice{0};
    uint64_t subtree_reinsert{0};
    uint64_t rename_key_updates{0};
    uint64_t tree_write_lock_ns{0};
    uint64_t reader_blocked_ns{0};
};

struct MemoryStats {
    uint64_t total_bytes{0};
    uint64_t logical_path_bytes{0};
    uint64_t unique_trie_bytes{0};
    uint64_t duplicated_prefix_bytes{0};
    uint64_t node_prefix_bytes{0};
    uint64_t node_suffix_bytes{0};
    uint64_t metadata_overhead_bytes{0};
    uint64_t node_count{0};
    uint64_t entry_count{0};
};

struct LatencySummary {
    double avg_ns{0.0};
    double p50_ns{0.0};
    double p95_ns{0.0};
    double p99_ns{0.0};
};

LatencySummary SummarizeLatencies(std::vector<uint64_t> latencies_ns);

}  // namespace flatbench
