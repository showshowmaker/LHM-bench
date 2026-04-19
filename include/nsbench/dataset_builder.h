#pragma once

#include "nsbench/common_types.h"
#include "nsbench/dataset_format.h"

#include <string>
#include <vector>

namespace nsbench {

struct DatasetBuildOptions {
    std::string dataset_name{"deep_tree"};
    std::vector<uint32_t> depths;
    uint32_t siblings_per_dir{8};
    uint32_t files_per_leaf{32};
    uint32_t positive_queries_per_depth{10000};
    uint32_t negative_queries_per_depth{10000};
    uint32_t name_width{4};
    uint64_t inode_start{1};
    uint32_t seed{1};
    std::string output_root;
};

struct BuiltDataset {
    DatasetManifest manifest;
    std::vector<NamespaceRecord> records;
    std::vector<QueryRecord> positive_queries;
    std::vector<QueryRecord> negative_queries;
};

class DatasetBuilder {
public:
    bool BuildOne(const DatasetBuildOptions& options,
                  uint32_t depth,
                  BuiltDataset* out,
                  std::string* error) const;

    bool BuildAll(const DatasetBuildOptions& options,
                  std::vector<BuiltDataset>* out,
                  std::string* error) const;

private:
    bool BuildNamespace(uint32_t depth,
                        uint32_t siblings_per_dir,
                        uint32_t files_per_leaf,
                        uint32_t name_width,
                        uint64_t inode_start,
                        std::vector<NamespaceRecord>* records,
                        std::string* error) const;

    bool BuildPositiveQueries(const std::vector<NamespaceRecord>& records,
                              uint32_t target_count,
                              uint32_t seed,
                              std::vector<QueryRecord>* queries,
                              std::string* error) const;

    bool BuildNegativeQueries(const std::vector<NamespaceRecord>& records,
                              uint32_t target_count,
                              uint32_t seed,
                              std::vector<QueryRecord>* queries,
                              std::string* error) const;
};

}  // namespace nsbench
