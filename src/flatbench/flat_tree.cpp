#include "flatbench/flat_tree.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>

namespace flatbench {

namespace {

class ScopedLockTimer {
public:
    explicit ScopedLockTimer(SpinMutex* mutex)
        : mutex_(mutex),
          wait_begin_(std::chrono::steady_clock::now()) {
        mutex_->lock();
        hold_begin_ = std::chrono::steady_clock::now();
    }

    ~ScopedLockTimer() {
        if (mutex_) {
            mutex_->unlock();
        }
    }

    uint64_t wait_ns() const {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(hold_begin_ - wait_begin_).count());
    }

    uint64_t hold_ns() const {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - hold_begin_)
                .count());
    }

private:
    SpinMutex* mutex_;
    std::chrono::steady_clock::time_point wait_begin_;
    std::chrono::steady_clock::time_point hold_begin_;
};

bool EntryLess(const FlatEntry& lhs, const FlatEntry& rhs) {
    return lhs.full_key < rhs.full_key;
}

bool KeyLess(const FlatEntry& entry, const std::string& key) {
    return entry.full_key < key;
}

struct TrieNode {
    std::map<char, TrieNode> children;
};

void InsertIntoTrie(TrieNode* root, const std::string& key) {
    TrieNode* node = root;
    for (size_t i = 0; i < key.size(); ++i) {
        node = &node->children[key[i]];
    }
}

uint64_t TrieBytes(const TrieNode& node) {
    uint64_t bytes = 0;
    for (std::map<char, TrieNode>::const_iterator it = node.children.begin();
         it != node.children.end();
         ++it) {
        bytes += 1;
        bytes += TrieBytes(it->second);
    }
    return bytes;
}

}  // namespace

FlatTree::FlatTree(size_t leaf_capacity)
    : leaf_capacity_(std::max<size_t>(leaf_capacity, 2)) {
}

bool FlatTree::Build(const std::vector<nsbench::NamespaceRecord>& records,
                     std::string* error) {
    OpStats op;
    ScopedLockTimer guard(&mutex_);

    std::vector<FlatEntry> entries;
    entries.reserve(records.size());
    for (size_t i = 0; i < records.size(); ++i) {
        if (records[i].path == "/") {
            continue;
        }
        FlatEntry entry;
        entry.full_key = records[i].prepared.normalized;
        entry.inode_id = records[i].inode_id;
        entry.type = records[i].type;
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), EntryLess);
    entries.erase(std::unique(entries.begin(), entries.end(),
                              [](const FlatEntry& lhs, const FlatEntry& rhs) {
                                  return lhs.full_key == rhs.full_key;
                              }),
                  entries.end());

    leaves_.clear();
    RebuildLeavesUnlocked(entries, &op);
    counters_.tree_write_lock_ns += guard.hold_ns();
    if (error) {
        error->clear();
    }
    return true;
}

bool FlatTree::Lookup(const nsbench::PreparedPath& path,
                      OpStats* out,
                      std::string* error) const {
    if (!out) {
        if (error) {
            *error = "lookup stats output is null";
        }
        return false;
    }

    ScopedLockTimer guard(&mutex_);
    OpStats result;
    result.lock_wait_ns = guard.wait_ns();
    result.nodes_touched = leaves_.empty() ? 0 : 1;

    FlatEntry found;
    const size_t leaf_index = FindLeafIndexUnlocked(path.normalized);
    if (leaf_index < leaves_.size()) {
        result.ok = LookupInLeafUnlocked(leaves_[leaf_index], path.normalized, &found);
    } else {
        result.ok = false;
    }

    result.lock_hold_ns = guard.hold_ns();
    counters_.reader_blocked_ns += result.lock_wait_ns;
    *out = result;
    if (error) {
        error->clear();
    }
    return true;
}

bool FlatTree::Insert(const nsbench::NamespaceRecord& record,
                      OpStats* out,
                      std::string* error) {
    if (!out) {
        if (error) {
            *error = "insert stats output is null";
        }
        return false;
    }
    if (record.prepared.normalized.empty() || record.prepared.normalized == "/") {
        if (error) {
            *error = "insert path is invalid";
        }
        return false;
    }

    ScopedLockTimer guard(&mutex_);
    OpStats result;
    result.lock_wait_ns = guard.wait_ns();

    FlatEntry entry;
    entry.full_key = record.prepared.normalized;
    entry.inode_id = record.inode_id;
    entry.type = record.type;

    if (leaves_.empty()) {
        FlatNode node;
        node.entries.push_back(entry);
        leaves_.push_back(node);
        RecomputeNodeUnlocked(&leaves_.back(), &result);
        result.ok = true;
        result.nodes_touched = 1;
    } else {
        const size_t leaf_index = FindLeafIndexUnlocked(entry.full_key);
        FlatNode& node = leaves_[std::min(leaf_index, leaves_.size() - 1)];
        std::vector<FlatEntry>::iterator pos =
            std::lower_bound(node.entries.begin(), node.entries.end(), entry.full_key, KeyLess);
        if (pos != node.entries.end() && pos->full_key == entry.full_key) {
            if (error) {
                *error = "duplicate key insert: " + entry.full_key;
            }
            return false;
        }
        node.entries.insert(pos, entry);
        RecomputeNodeUnlocked(&node, &result);
        result.ok = true;
        result.nodes_touched = 1;
        if (node.entries.size() > leaf_capacity_) {
            SplitLeafUnlocked(std::min(leaf_index, leaves_.size() - 1), &result);
        }
    }

    result.lock_hold_ns = guard.hold_ns();
    counters_.tree_write_lock_ns += result.lock_hold_ns;
    *out = result;
    if (error) {
        error->clear();
    }
    return true;
}

bool FlatTree::Erase(const nsbench::PreparedPath& path,
                     OpStats* out,
                     std::string* error) {
    if (!out) {
        if (error) {
            *error = "erase stats output is null";
        }
        return false;
    }

    ScopedLockTimer guard(&mutex_);
    OpStats result;
    result.lock_wait_ns = guard.wait_ns();

    if (leaves_.empty()) {
        result.ok = false;
    } else {
        const size_t leaf_index = FindLeafIndexUnlocked(path.normalized);
        if (leaf_index < leaves_.size()) {
            FlatNode& node = leaves_[leaf_index];
            std::vector<FlatEntry>::iterator pos =
                std::lower_bound(node.entries.begin(), node.entries.end(), path.normalized, KeyLess);
            if (pos != node.entries.end() && pos->full_key == path.normalized) {
                node.entries.erase(pos);
                result.ok = true;
                result.nodes_touched = 1;
                if (node.entries.empty()) {
                    leaves_.erase(leaves_.begin() + static_cast<std::ptrdiff_t>(leaf_index));
                } else {
                    RecomputeNodeUnlocked(&node, &result);
                    MaybeMergeLeafUnlocked(leaf_index, &result);
                }
            }
        }
    }

    result.lock_hold_ns = guard.hold_ns();
    counters_.tree_write_lock_ns += result.lock_hold_ns;
    *out = result;
    if (error) {
        error->clear();
    }
    return true;
}

bool FlatTree::RenameSubtree(const nsbench::PreparedPath& src,
                             const nsbench::PreparedPath& dst,
                             OpStats* out,
                             std::string* error) {
    if (!out) {
        if (error) {
            *error = "rename stats output is null";
        }
        return false;
    }
    if (src.normalized.empty() || dst.normalized.empty()) {
        if (error) {
            *error = "rename prefixes are invalid";
        }
        return false;
    }

    ScopedLockTimer guard(&mutex_);
    OpStats result;
    result.lock_wait_ns = guard.wait_ns();

    std::vector<FlatEntry> entries = CollectEntriesUnlocked();
    size_t renamed = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (!IsSubtreePath(entries[i].full_key, src.normalized)) {
            continue;
        }
        const std::string suffix = entries[i].full_key.substr(src.normalized.size());
        result.bytes_rewritten += static_cast<uint64_t>(entries[i].full_key.size() + dst.normalized.size() +
                                                        suffix.size());
        entries[i].full_key = dst.normalized + suffix;
        ++renamed;
    }

    if (renamed == 0) {
        result.ok = false;
        result.lock_hold_ns = guard.hold_ns();
        counters_.tree_write_lock_ns += result.lock_hold_ns;
        *out = result;
        if (error) {
            *error = "rename subtree source not found: " + src.normalized;
        }
        return false;
    }

    counters_.subtree_slice += renamed;
    counters_.subtree_reinsert += renamed;
    counters_.rename_key_updates += renamed;
    std::sort(entries.begin(), entries.end(), EntryLess);
    RebuildLeavesUnlocked(entries, &result);
    result.ok = true;
    result.nodes_touched = leaves_.size();
    result.bytes_moved += renamed;
    result.lock_hold_ns = guard.hold_ns();
    counters_.tree_write_lock_ns += result.lock_hold_ns;
    *out = result;
    if (error) {
        error->clear();
    }
    return true;
}

MemoryStats FlatTree::SnapshotMemory() const {
    ScopedLockTimer guard(&mutex_);
    return SnapshotMemoryUnlocked();
}

FlatInternalCounters FlatTree::Counters() const {
    ScopedLockTimer guard(&mutex_);
    return counters_;
}

void FlatTree::ResetCounters() {
    ScopedLockTimer guard(&mutex_);
    counters_ = FlatInternalCounters();
}

size_t FlatTree::leaf_count() const {
    ScopedLockTimer guard(&mutex_);
    return leaves_.size();
}

size_t FlatTree::entry_count() const {
    ScopedLockTimer guard(&mutex_);
    size_t total = 0;
    for (size_t i = 0; i < leaves_.size(); ++i) {
        total += leaves_[i].entries.size();
    }
    return total;
}

size_t FlatTree::FindLeafIndexUnlocked(const std::string& key) const {
    if (leaves_.empty()) {
        return 0;
    }
    for (size_t i = 0; i < leaves_.size(); ++i) {
        const FlatNode& node = leaves_[i];
        if (node.entries.empty()) {
            continue;
        }
        if (key <= node.entries.back().full_key) {
            return i;
        }
    }
    return leaves_.size() - 1;
}

bool FlatTree::LookupInLeafUnlocked(const FlatNode& node,
                                    const std::string& key,
                                    FlatEntry* out) const {
    std::vector<FlatEntry>::const_iterator it =
        std::lower_bound(node.entries.begin(), node.entries.end(), key, KeyLess);
    if (it == node.entries.end() || it->full_key != key) {
        return false;
    }
    if (out) {
        *out = *it;
    }
    return true;
}

void FlatTree::RecomputeNodeUnlocked(FlatNode* node, OpStats* op) {
    if (!node) {
        return;
    }
    const std::string old_prefix = node->common_prefix;
    const uint64_t old_bytes = EncodedNodeBytes(*node);
    const uint64_t old_suffix_bytes = NodeSuffixBytes(*node);

    const size_t prefix_len = LongestCommonPrefix(node->entries);
    node->common_prefix.assign(node->entries.empty() ? std::string() : node->entries[0].full_key.substr(0, prefix_len));

    const uint64_t new_bytes = EncodedNodeBytes(*node);
    const uint64_t new_suffix_bytes = NodeSuffixBytes(*node);

    ++counters_.node_prefix_recompute;
    counters_.entry_reencoded += node->entries.size();
    if (old_prefix != node->common_prefix) {
        ++counters_.prefix_change_count;
        if (node->common_prefix.size() >= old_prefix.size()) {
            counters_.prefix_expand_bytes += node->common_prefix.size() - old_prefix.size();
        } else {
            counters_.prefix_shrink_bytes += old_prefix.size() - node->common_prefix.size();
        }
    }
    if (new_suffix_bytes >= old_suffix_bytes) {
        counters_.suffix_expand_bytes += new_suffix_bytes - old_suffix_bytes;
    } else {
        counters_.suffix_shrink_bytes += old_suffix_bytes - new_suffix_bytes;
    }
    if (op) {
        op->bytes_rewritten += old_bytes + new_bytes;
    }
}

void FlatTree::SplitLeafUnlocked(size_t leaf_index, OpStats* op) {
    if (leaf_index >= leaves_.size() || leaves_[leaf_index].entries.size() <= leaf_capacity_) {
        return;
    }
    FlatNode old = leaves_[leaf_index];
    const size_t mid = old.entries.size() / 2;

    FlatNode left;
    left.entries.assign(old.entries.begin(), old.entries.begin() + static_cast<std::ptrdiff_t>(mid));
    FlatNode right;
    right.entries.assign(old.entries.begin() + static_cast<std::ptrdiff_t>(mid), old.entries.end());

    leaves_[leaf_index] = left;
    leaves_.insert(leaves_.begin() + static_cast<std::ptrdiff_t>(leaf_index + 1), right);
    RecomputeNodeUnlocked(&leaves_[leaf_index], op);
    RecomputeNodeUnlocked(&leaves_[leaf_index + 1], op);
    ++counters_.node_split;
    if (op) {
        op->nodes_touched += 2;
        op->bytes_moved += EncodedNodeBytes(old);
    }
}

void FlatTree::MaybeMergeLeafUnlocked(size_t leaf_index, OpStats* op) {
    if (leaves_.size() < 2 || leaf_index >= leaves_.size()) {
        return;
    }

    size_t neighbor_index = leaf_index;
    if (leaf_index + 1 < leaves_.size()) {
        neighbor_index = leaf_index + 1;
    } else if (leaf_index > 0) {
        neighbor_index = leaf_index - 1;
    } else {
        return;
    }

    const size_t first = std::min(leaf_index, neighbor_index);
    const size_t second = std::max(leaf_index, neighbor_index);
    if (leaves_[first].entries.size() + leaves_[second].entries.size() > leaf_capacity_) {
        return;
    }

    FlatNode merged;
    merged.entries.reserve(leaves_[first].entries.size() + leaves_[second].entries.size());
    merged.entries.insert(merged.entries.end(), leaves_[first].entries.begin(), leaves_[first].entries.end());
    merged.entries.insert(merged.entries.end(), leaves_[second].entries.begin(), leaves_[second].entries.end());
    std::sort(merged.entries.begin(), merged.entries.end(), EntryLess);

    const uint64_t old_bytes = EncodedNodeBytes(leaves_[first]) + EncodedNodeBytes(leaves_[second]);
    leaves_[first] = merged;
    leaves_.erase(leaves_.begin() + static_cast<std::ptrdiff_t>(second));
    RecomputeNodeUnlocked(&leaves_[first], op);
    ++counters_.node_merge;
    if (op) {
        op->nodes_touched += 2;
        op->bytes_moved += old_bytes;
    }
}

void FlatTree::RebuildLeavesUnlocked(const std::vector<FlatEntry>& entries,
                                     OpStats* op) {
    leaves_.clear();
    FlatNode current;
    for (size_t i = 0; i < entries.size(); ++i) {
        current.entries.push_back(entries[i]);
        if (current.entries.size() == leaf_capacity_) {
            leaves_.push_back(current);
            current = FlatNode();
        }
    }
    if (!current.entries.empty()) {
        leaves_.push_back(current);
    }
    for (size_t i = 0; i < leaves_.size(); ++i) {
        RecomputeNodeUnlocked(&leaves_[i], op);
    }
}

std::vector<FlatEntry> FlatTree::CollectEntriesUnlocked() const {
    std::vector<FlatEntry> entries;
    for (size_t i = 0; i < leaves_.size(); ++i) {
        entries.insert(entries.end(), leaves_[i].entries.begin(), leaves_[i].entries.end());
    }
    return entries;
}

MemoryStats FlatTree::SnapshotMemoryUnlocked() const {
    MemoryStats stats;
    stats.node_count = leaves_.size();

    std::set<std::string> unique_prefixes;
    uint64_t unique_prefix_bytes = 0;
    TrieNode trie_root;

    for (size_t i = 0; i < leaves_.size(); ++i) {
        const FlatNode& node = leaves_[i];
        stats.entry_count += node.entries.size();
        stats.node_prefix_bytes += NodePrefixBytes(node);
        stats.node_suffix_bytes += NodeSuffixBytes(node);
        stats.logical_path_bytes += LogicalPathBytes(node);
        stats.metadata_overhead_bytes += kFlatNodeHeaderBytes;
        if (unique_prefixes.insert(node.common_prefix).second) {
            unique_prefix_bytes += node.common_prefix.size();
        }
        for (size_t j = 0; j < node.entries.size(); ++j) {
            stats.metadata_overhead_bytes += kFlatEntryHeaderBytes;
            InsertIntoTrie(&trie_root, node.entries[j].full_key);
        }
    }
    stats.unique_trie_bytes = TrieBytes(trie_root);
    if (stats.node_prefix_bytes > unique_prefix_bytes) {
        stats.duplicated_prefix_bytes = stats.node_prefix_bytes - unique_prefix_bytes;
    }
    stats.total_bytes = stats.node_prefix_bytes + stats.node_suffix_bytes + stats.metadata_overhead_bytes;
    return stats;
}

bool FlatTree::IsSubtreePath(const std::string& path, const std::string& prefix) {
    if (prefix == "/") {
        return true;
    }
    if (path == prefix) {
        return true;
    }
    if (path.size() <= prefix.size()) {
        return false;
    }
    return path.compare(0, prefix.size(), prefix) == 0 && path[prefix.size()] == '/';
}

size_t FlatTree::LongestCommonPrefix(const std::vector<FlatEntry>& entries) {
    if (entries.empty()) {
        return 0;
    }
    const std::string& first = entries.front().full_key;
    const std::string& last = entries.back().full_key;
    const size_t limit = std::min(first.size(), last.size());
    size_t i = 0;
    for (; i < limit; ++i) {
        if (first[i] != last[i]) {
            break;
        }
    }
    return i;
}

uint64_t FlatTree::EncodedNodeBytes(const FlatNode& node) {
    return kFlatNodeHeaderBytes + NodePrefixBytes(node) + NodeSuffixBytes(node) +
           static_cast<uint64_t>(node.entries.size()) * kFlatEntryHeaderBytes;
}

uint64_t FlatTree::NodePrefixBytes(const FlatNode& node) {
    return static_cast<uint64_t>(node.common_prefix.size());
}

uint64_t FlatTree::NodeSuffixBytes(const FlatNode& node) {
    uint64_t total = 0;
    for (size_t i = 0; i < node.entries.size(); ++i) {
        total += static_cast<uint64_t>(
            node.entries[i].full_key.size() >= node.common_prefix.size()
                ? node.entries[i].full_key.size() - node.common_prefix.size()
                : node.entries[i].full_key.size());
    }
    return total;
}

uint64_t FlatTree::LogicalPathBytes(const FlatNode& node) {
    uint64_t total = 0;
    for (size_t i = 0; i < node.entries.size(); ++i) {
        total += static_cast<uint64_t>(node.entries[i].full_key.size());
    }
    return total;
}

}  // namespace flatbench
