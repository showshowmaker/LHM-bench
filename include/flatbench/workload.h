#pragma once

#include "nsbench/common_types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace flatbench {

bool MakeRecord(uint64_t inode_id,
                nsbench::NodeType type,
                const std::string& path,
                nsbench::NamespaceRecord* out,
                std::string* error);

bool BuildPrefixRecords(uint32_t depth,
                        uint32_t siblings_per_dir,
                        uint32_t files_per_leaf,
                        std::vector<nsbench::NamespaceRecord>* records,
                        std::string* error);

bool BuildChurnWorkload(size_t steady_keys,
                        std::vector<nsbench::NamespaceRecord>* records,
                        nsbench::NamespaceRecord* stable_candidate,
                        nsbench::NamespaceRecord* oscillating_candidate,
                        std::string* error);

bool BuildRenameWorkload(size_t subtree_files,
                         std::vector<nsbench::NamespaceRecord>* records,
                         nsbench::PreparedPath* src_prefix,
                         nsbench::PreparedPath* dst_prefix,
                         std::vector<nsbench::PreparedPath>* lookup_paths,
                         std::string* error);

}  // namespace flatbench
