#pragma once

#include "flatbench/backend.h"
#include "flatbench/flat_node.h"

#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace flatbench {

class SpinMutex {
public:
    SpinMutex() : flag_(ATOMIC_FLAG_INIT) {
    }

    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_;
};

class FlatTree {
public:
    explicit FlatTree(size_t leaf_capacity);

    bool Build(const std::vector<nsbench::NamespaceRecord>& records,
               std::string* error);

    bool Lookup(const nsbench::PreparedPath& path,
                OpStats* out,
                std::string* error) const;

    bool Insert(const nsbench::NamespaceRecord& record,
                OpStats* out,
                std::string* error);

    bool Erase(const nsbench::PreparedPath& path,
               OpStats* out,
               std::string* error);

    bool RenameSubtree(const nsbench::PreparedPath& src,
                       const nsbench::PreparedPath& dst,
                       OpStats* out,
                       std::string* error);

    MemoryStats SnapshotMemory() const;
    FlatInternalCounters Counters() const;
    void ResetCounters();
    size_t leaf_count() const;
    size_t entry_count() const;

private:
    typedef std::vector<FlatNode>::iterator NodeIter;
    typedef std::vector<FlatNode>::const_iterator ConstNodeIter;

    size_t FindLeafIndexUnlocked(const std::string& key) const;
    bool LookupInLeafUnlocked(const FlatNode& node, const std::string& key, FlatEntry* out) const;
    void RecomputeNodeUnlocked(FlatNode* node, OpStats* op);
    void SplitLeafUnlocked(size_t leaf_index, OpStats* op);
    void MaybeMergeLeafUnlocked(size_t leaf_index, OpStats* op);
    void RebuildLeavesUnlocked(const std::vector<FlatEntry>& entries, OpStats* op);
    std::vector<FlatEntry> CollectEntriesUnlocked() const;
    MemoryStats SnapshotMemoryUnlocked() const;

    static bool IsSubtreePath(const std::string& path, const std::string& prefix);
    static size_t LongestCommonPrefix(const std::vector<FlatEntry>& entries);
    static uint64_t EncodedNodeBytes(const FlatNode& node);
    static uint64_t NodePrefixBytes(const FlatNode& node);
    static uint64_t NodeSuffixBytes(const FlatNode& node);
    static uint64_t LogicalPathBytes(const FlatNode& node);

private:
    size_t leaf_capacity_;
    std::vector<FlatNode> leaves_;
    mutable SpinMutex mutex_;
    mutable FlatInternalCounters counters_;
};

}  // namespace flatbench
