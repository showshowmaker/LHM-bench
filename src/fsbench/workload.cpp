#include "fsbench/workload.h"

#include "fsbench/path_utils.h"

#include <cstdio>
#include <random>

namespace fsbench {
namespace {

std::string MakeName(char prefix, uint32_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%c%04u", prefix, value);
    return std::string(buf);
}

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

    uint64_t current_parent = 1;
    std::string current_path;
    for (uint32_t level = 1; level <= options.depth; ++level) {
        const std::string dir_name = MakeName('d', level);
        current_path += "/" + dir_name;
        const uint64_t current_inode = next_inode++;
        AddEntry(current_inode, current_parent, NodeType::kDirectory, current_path, &result.entries);

        for (uint32_t sibling = 0; sibling < options.siblings_per_dir; ++sibling) {
            const std::string sibling_path =
                current_path + "/" + MakeName('s', sibling);
            AddEntry(next_inode++, current_inode, NodeType::kDirectory, sibling_path, &result.entries);
        }

        current_parent = current_inode;
    }

    for (uint32_t file_index = 0; file_index < options.files_per_leaf; ++file_index) {
        const std::string file_path =
            current_path + "/" + MakeName('f', file_index) + ".dat";
        AddEntry(next_inode++, current_parent, NodeType::kFile, file_path, &result.entries);
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
