#include "fsbench/workload.h"

#include "fsbench/path_utils.h"

#include <cstdio>
#include <limits>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fsbench {
namespace {

void AddEntry(uint64_t inode_id,
              uint64_t parent_inode_id,
              NodeType type,
              const std::string& path,
              std::vector<NamespaceEntry>* entries) {
    NamespaceEntry entry;
    entry.inode_id = inode_id;
    entry.parent_inode_id = parent_inode_id;
    entry.type = type;
    entry.path = path;
    PreparePath(path, &entry.prepared, nullptr);
    entries->push_back(std::move(entry));
}

std::string MakeDirName(uint32_t level, uint32_t slot) {
    char buf[48];
    snprintf(buf, sizeof(buf), "d%02u_%u", level, slot);
    return std::string(buf);
}

std::string MakeFileName(uint64_t value) {
    char buf[48];
    snprintf(buf, sizeof(buf), "f%010llu.dat", static_cast<unsigned long long>(value));
    return std::string(buf);
}

uint64_t SafeMulSaturating(uint64_t a, uint64_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    if (a > std::numeric_limits<uint64_t>::max() / b) {
        return std::numeric_limits<uint64_t>::max();
    }
    return a * b;
}

uint64_t SafePowSaturating(uint64_t base, uint32_t exp) {
    uint64_t value = 1;
    for (uint32_t i = 0; i < exp; ++i) {
        value = SafeMulSaturating(value, base);
        if (value == std::numeric_limits<uint64_t>::max()) {
            break;
        }
    }
    return value;
}

uint64_t CeilDiv(uint64_t a, uint64_t b) {
    return b == 0 ? 0 : (a + b - 1) / b;
}

std::vector<uint32_t> LeafDigits(uint64_t leaf_index, uint32_t depth, uint32_t branch_factor) {
    std::vector<uint32_t> digits(depth, 0);
    for (uint32_t pos = 0; pos < depth; ++pos) {
        const uint32_t rev = depth - 1 - pos;
        digits[rev] = static_cast<uint32_t>(leaf_index % branch_factor);
        leaf_index /= branch_factor;
    }
    return digits;
}

}  // namespace

bool WorkloadBuilder::Build(const WorkloadOptions& options,
                            WorkloadData* out,
                            std::string* error) const {
    if (!out) {
        if (error) {
            *error = "workload output is null";
        }
        return false;
    }

    WorkloadData result;
    uint64_t next_inode = 1;
    AddEntry(next_inode++, 0, NodeType::kDirectory, "/", &result.entries);

    const uint64_t target_files =
        options.target_file_count == 0 ? static_cast<uint64_t>(options.files_per_leaf)
                                       : options.target_file_count;
    const uint32_t max_files_per_leaf = options.files_per_leaf == 0 ? 1 : options.files_per_leaf;
    const uint32_t branch_factor = options.siblings_per_dir == 0 ? 1 : options.siblings_per_dir;
    const uint64_t leaf_dir_count = CeilDiv(target_files, max_files_per_leaf);
    const uint64_t max_leaf_capacity =
        options.depth == 0 ? 1 : SafePowSaturating(branch_factor, options.depth);

    if (leaf_dir_count == 0) {
        if (error) {
            *error = "target_file_count resolved to zero";
        }
        return false;
    }
    if (leaf_dir_count > max_leaf_capacity) {
        if (error) {
            *error = "depth and siblings_per_dir cannot host target_file_count files";
        }
        return false;
    }

    std::unordered_map<std::string, uint64_t> dir_to_inode;
    dir_to_inode.reserve(static_cast<size_t>(leaf_dir_count * (options.depth == 0 ? 1 : options.depth)));
    dir_to_inode["/"] = 1;

    std::vector<std::pair<std::string, uint64_t>> leaf_dirs;
    leaf_dirs.reserve(static_cast<size_t>(leaf_dir_count));

    if (options.depth == 0) {
        leaf_dirs.emplace_back("/", 1);
    } else {
        for (uint64_t leaf_index = 0; leaf_index < leaf_dir_count; ++leaf_index) {
            const std::vector<uint32_t> digits = LeafDigits(leaf_index, options.depth, branch_factor);
            std::string current_path;
            uint64_t current_parent = 1;
            for (uint32_t level = 0; level < options.depth; ++level) {
                const std::string dir_name = MakeDirName(level + 1, digits[level]);
                current_path += "/" + dir_name;
                auto it = dir_to_inode.find(current_path);
                if (it == dir_to_inode.end()) {
                    const uint64_t inode_id = next_inode++;
                    AddEntry(inode_id,
                             current_parent,
                             NodeType::kDirectory,
                             current_path,
                             &result.entries);
                    dir_to_inode.emplace(current_path, inode_id);
                    current_parent = inode_id;
                } else {
                    current_parent = it->second;
                }
            }
            leaf_dirs.emplace_back(current_path, current_parent);
        }
    }

    for (uint64_t file_index = 0; file_index < target_files; ++file_index) {
        const uint64_t leaf_index = file_index / max_files_per_leaf;
        const auto& leaf = leaf_dirs[static_cast<size_t>(leaf_index)];
        const std::string file_path =
            (leaf.first == "/" ? std::string() : leaf.first) + "/" + MakeFileName(file_index);
        AddEntry(next_inode++, leaf.second, NodeType::kFile, file_path, &result.entries);
    }

    std::vector<const NamespaceEntry*> files;
    files.reserve(result.entries.size());
    for (const NamespaceEntry& entry : result.entries) {
        if (entry.type == NodeType::kFile) {
            files.push_back(&entry);
        }
    }
    if (files.empty()) {
        if (error) {
            *error = "workload generated no file entries";
        }
        return false;
    }

    std::mt19937 rng(options.seed);
    std::uniform_int_distribution<size_t> file_pick(0, files.size() - 1);

    for (uint32_t i = 0; i < options.positive_queries; ++i) {
        Query q;
        q.prepared = files[file_pick(rng)]->prepared;
        q.expect_found = true;
        result.positive_queries.push_back(std::move(q));
    }

    for (uint32_t i = 0; i < options.negative_queries; ++i) {
        Query q;
        q.expect_found = false;
        q.prepared = files[file_pick(rng)]->prepared;
        if (!q.prepared.components.empty()) {
            q.prepared.components.back() += ".missing";
            q.prepared.normalized += ".missing";
        }
        result.negative_queries.push_back(std::move(q));
    }

    *out = std::move(result);
    if (error) {
        error->clear();
    }
    return true;
}

uint64_t CountFiles(const std::vector<NamespaceEntry>& entries) {
    uint64_t count = 0;
    for (const NamespaceEntry& entry : entries) {
        if (entry.type == NodeType::kFile) {
            ++count;
        }
    }
    return count;
}

}  // namespace fsbench
