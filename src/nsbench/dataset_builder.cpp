#include "nsbench/dataset_builder.h"

#include "nsbench/path_utils.h"

#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

namespace nsbench {

namespace {

std::string PathJoin(const std::string& base, const std::string& leaf) {
    if (base.empty()) {
        return leaf;
    }
    if (base.back() == '/' || base.back() == '\\') {
        return base + leaf;
    }
    return base + "/" + leaf;
}

std::string JoinPath(const std::string& parent, const std::string& child) {
    return parent == "/" ? "/" + child : parent + "/" + child;
}

std::string MakeName(char prefix, uint32_t a, uint32_t b, uint32_t width) {
    std::ostringstream out;
    out << prefix << std::setw(static_cast<int>(width)) << std::setfill('0') << a;
    if (b != 0) {
        out << "_" << std::setw(static_cast<int>(width)) << std::setfill('0') << b;
    }
    return out.str();
}

bool AddRecord(uint64_t inode_id,
               uint64_t parent_inode_id,
               NodeType type,
               const std::string& path,
               std::vector<NamespaceRecord>* records,
               std::string* error) {
    NamespaceRecord record;
    record.inode_id = inode_id;
    record.parent_inode_id = parent_inode_id;
    record.type = type;
    record.path = path;
    if (!PreparePath(path, &record.prepared, error)) {
        return false;
    }
    records->push_back(std::move(record));
    return true;
}

}  // namespace

bool DatasetBuilder::BuildOne(const DatasetBuildOptions& options,
                              uint32_t depth,
                              BuiltDataset* out,
                              std::string* error) const {
    if (!out) {
        if (error) {
            *error = "built dataset output is null";
        }
        return false;
    }

    BuiltDataset built;
    if (!BuildNamespace(depth,
                        options.siblings_per_dir,
                        options.files_per_leaf,
                        options.name_width,
                        options.inode_start,
                        &built.records,
                        error)) {
        return false;
    }
    if (!BuildPositiveQueries(built.records,
                              options.positive_queries_per_depth,
                              options.seed + depth,
                              &built.positive_queries,
                              error)) {
        return false;
    }
    if (!BuildNegativeQueries(built.records,
                              options.negative_queries_per_depth,
                              options.seed + depth * 17U,
                              &built.negative_queries,
                              error)) {
        return false;
    }

    const std::string depth_dir = "depth_" + [&]() {
        std::ostringstream name;
        name << std::setw(2) << std::setfill('0') << depth;
        return name.str();
    }();
    const std::string root = PathJoin(options.output_root, depth_dir);
    built.manifest.dataset_name = options.dataset_name;
    built.manifest.depth = depth;
    built.manifest.siblings_per_dir = options.siblings_per_dir;
    built.manifest.files_per_leaf = options.files_per_leaf;
    built.manifest.total_records = built.records.size();
    built.manifest.total_queries = built.positive_queries.size() + built.negative_queries.size();
    built.manifest.records_tsv = PathJoin(root, "records.tsv");
    built.manifest.positive_queries_tsv = PathJoin(root, "positive_queries.tsv");
    built.manifest.negative_queries_tsv = PathJoin(root, "negative_queries.tsv");

    *out = std::move(built);
    if (error) {
        error->clear();
    }
    return true;
}

bool DatasetBuilder::BuildAll(const DatasetBuildOptions& options,
                              std::vector<BuiltDataset>* out,
                              std::string* error) const {
    if (!out) {
        if (error) {
            *error = "dataset vector output is null";
        }
        return false;
    }
    out->clear();
    for (uint32_t depth : options.depths) {
        BuiltDataset dataset;
        if (!BuildOne(options, depth, &dataset, error)) {
            return false;
        }
        out->push_back(std::move(dataset));
    }
    return true;
}

bool DatasetBuilder::BuildNamespace(uint32_t depth,
                                    uint32_t siblings_per_dir,
                                    uint32_t files_per_leaf,
                                    uint32_t name_width,
                                    uint64_t inode_start,
                                    std::vector<NamespaceRecord>* records,
                                    std::string* error) const {
    if (!records || inode_start == 0) {
        if (error) {
            *error = "invalid namespace build args";
        }
        return false;
    }

    records->clear();
    uint64_t next_inode = inode_start;
    if (!AddRecord(next_inode++, 0, NodeType::kDirectory, "/", records, error)) {
        return false;
    }

    std::string parent_path = "/";
    uint64_t parent_inode = inode_start;

    for (uint32_t level = 1; level <= depth; ++level) {
        const std::string main_name = MakeName('d', level, 0, name_width);
        const std::string main_path = JoinPath(parent_path, main_name);
        const uint64_t main_inode = next_inode++;
        if (!AddRecord(main_inode, parent_inode, NodeType::kDirectory, main_path, records, error)) {
            return false;
        }

        for (uint32_t sibling = 1; sibling <= siblings_per_dir; ++sibling) {
            const std::string sibling_name = MakeName('s', level, sibling, name_width);
            const std::string sibling_path = JoinPath(parent_path, sibling_name);
            const uint64_t sibling_inode = next_inode++;
            if (!AddRecord(sibling_inode, parent_inode, NodeType::kDirectory, sibling_path, records, error)) {
                return false;
            }
        }

        parent_path = main_path;
        parent_inode = main_inode;
    }

    for (uint32_t file_index = 1; file_index <= files_per_leaf; ++file_index) {
        const std::string file_name = MakeName('f', file_index, 0, name_width) + ".dat";
        const std::string file_path = JoinPath(parent_path, file_name);
        if (!AddRecord(next_inode++, parent_inode, NodeType::kFile, file_path, records, error)) {
            return false;
        }
    }

    return true;
}

bool DatasetBuilder::BuildPositiveQueries(const std::vector<NamespaceRecord>& records,
                                          uint32_t target_count,
                                          uint32_t seed,
                                          std::vector<QueryRecord>* queries,
                                          std::string* error) const {
    if (!queries) {
        if (error) {
            *error = "positive queries output is null";
        }
        return false;
    }

    queries->clear();
    std::vector<const NamespaceRecord*> candidates;
    candidates.reserve(records.size());
    for (const NamespaceRecord& record : records) {
        if (record.path != "/") {
            candidates.push_back(&record);
        }
    }
    if (candidates.empty()) {
        return true;
    }

    std::mt19937 rng(seed);
    std::shuffle(candidates.begin(), candidates.end(), rng);
    const size_t limit = std::min<size_t>(target_count, candidates.size());
    queries->reserve(limit);
    for (size_t i = 0; i < limit; ++i) {
        QueryRecord query;
        query.path = candidates[i]->path;
        query.prepared = candidates[i]->prepared;
        query.expect_found = true;
        query.expected_inode_id = candidates[i]->inode_id;
        query.expected_depth = candidates[i]->prepared.depth();
        queries->push_back(std::move(query));
    }
    return true;
}

bool DatasetBuilder::BuildNegativeQueries(const std::vector<NamespaceRecord>& records,
                                          uint32_t target_count,
                                          uint32_t seed,
                                          std::vector<QueryRecord>* queries,
                                          std::string* error) const {
    if (!queries) {
        if (error) {
            *error = "negative queries output is null";
        }
        return false;
    }

    queries->clear();
    std::vector<const NamespaceRecord*> directories;
    directories.reserve(records.size());
    for (const NamespaceRecord& record : records) {
        if (record.type == NodeType::kDirectory) {
            directories.push_back(&record);
        }
    }
    if (directories.empty()) {
        return true;
    }

    std::mt19937 rng(seed);
    std::shuffle(directories.begin(), directories.end(), rng);
    const size_t limit = std::min<size_t>(target_count, directories.size());
    queries->reserve(limit);
    for (size_t i = 0; i < limit; ++i) {
        const NamespaceRecord& dir = *directories[i];
        QueryRecord query;
        query.path = JoinPath(dir.path, MakeName('m', static_cast<uint32_t>(i + 1), 0, 4));
        if (!PreparePath(query.path, &query.prepared, error)) {
            return false;
        }
        query.expect_found = false;
        query.expected_inode_id = 0;
        query.expected_depth = query.prepared.depth();
        queries->push_back(std::move(query));
    }
    return true;
}

}  // namespace nsbench
