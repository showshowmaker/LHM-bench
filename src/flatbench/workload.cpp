#include "flatbench/workload.h"

#include "nsbench/dataset_builder.h"
#include "nsbench/path_utils.h"

#include <algorithm>
#include <sstream>

namespace flatbench {

namespace {

std::string JoinPath(const std::string& parent, const std::string& child) {
    return parent == "/" ? "/" + child : parent + "/" + child;
}

std::string NumName(const std::string& prefix, size_t value) {
    std::ostringstream out;
    out << prefix << value;
    return out.str();
}

}  // namespace

bool MakeRecord(uint64_t inode_id,
                nsbench::NodeType type,
                const std::string& path,
                nsbench::NamespaceRecord* out,
                std::string* error) {
    if (!out) {
        if (error) {
            *error = "record output is null";
        }
        return false;
    }
    nsbench::NamespaceRecord record;
    record.inode_id = inode_id;
    record.parent_inode_id = 0;
    record.type = type;
    record.path = path;
    if (!nsbench::PreparePath(path, &record.prepared, error)) {
        return false;
    }
    *out = record;
    if (error) {
        error->clear();
    }
    return true;
}

bool BuildPrefixRecords(uint32_t depth,
                        uint32_t siblings_per_dir,
                        uint32_t files_per_leaf,
                        std::vector<nsbench::NamespaceRecord>* records,
                        std::string* error) {
    if (!records) {
        if (error) {
            *error = "prefix records output is null";
        }
        return false;
    }

    nsbench::DatasetBuildOptions options;
    options.depths.push_back(depth);
    options.siblings_per_dir = siblings_per_dir;
    options.files_per_leaf = files_per_leaf;
    options.positive_queries_per_depth = files_per_leaf;
    options.negative_queries_per_depth = files_per_leaf;
    options.output_root = ".";

    nsbench::BuiltDataset built;
    nsbench::DatasetBuilder builder;
    if (!builder.BuildOne(options, depth, &built, error)) {
        return false;
    }

    records->clear();
    for (size_t i = 0; i < built.records.size(); ++i) {
        if (built.records[i].path != "/") {
            records->push_back(built.records[i]);
        }
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool BuildChurnWorkload(size_t steady_keys,
                        std::vector<nsbench::NamespaceRecord>* records,
                        nsbench::NamespaceRecord* stable_candidate,
                        nsbench::NamespaceRecord* oscillating_candidate,
                        std::string* error) {
    if (!records || !stable_candidate || !oscillating_candidate) {
        if (error) {
            *error = "churn workload outputs are null";
        }
        return false;
    }

    records->clear();
    uint64_t inode = 1;
    for (size_t i = 0; i < steady_keys; ++i) {
        nsbench::NamespaceRecord file_record;
        if (!MakeRecord(inode++,
                        nsbench::NodeType::kFile,
                        JoinPath("/hot/common", NumName("aaaa", i)),
                        &file_record,
                        error)) {
            return false;
        }
        records->push_back(file_record);
    }

    if (!MakeRecord(inode++, nsbench::NodeType::kFile, "/hot/common/aaaz_stable", stable_candidate, error) ||
        !MakeRecord(inode++, nsbench::NodeType::kFile, "/hot/z_outlier", oscillating_candidate, error)) {
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool BuildRenameWorkload(size_t subtree_files,
                         std::vector<nsbench::NamespaceRecord>* records,
                         nsbench::PreparedPath* src_prefix,
                         nsbench::PreparedPath* dst_prefix,
                         std::vector<nsbench::PreparedPath>* lookup_paths,
                         std::string* error) {
    if (!records || !src_prefix || !dst_prefix || !lookup_paths) {
        if (error) {
            *error = "rename workload outputs are null";
        }
        return false;
    }

    records->clear();
    lookup_paths->clear();

    uint64_t inode = 1;
    const char* dirs[] = {"/src", "/src/project", "/src/project/data", "/dst", "/dst/archive"};
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        nsbench::NamespaceRecord record;
        if (!MakeRecord(inode++, nsbench::NodeType::kDirectory, dirs[i], &record, error)) {
            return false;
        }
        records->push_back(record);
    }

    for (size_t i = 0; i < subtree_files; ++i) {
        const std::string path = JoinPath("/src/project/data", NumName("file", i));
        nsbench::NamespaceRecord record;
        if (!MakeRecord(inode++, nsbench::NodeType::kFile, path, &record, error)) {
            return false;
        }
        records->push_back(record);
        if (lookup_paths->size() < 1024) {
            lookup_paths->push_back(record.prepared);
        }
    }

    if (!nsbench::PreparePath("/src/project", src_prefix, error) ||
        !nsbench::PreparePath("/dst/archive/project", dst_prefix, error)) {
        return false;
    }

    if (error) {
        error->clear();
    }
    return true;
}

}  // namespace flatbench
