#pragma once

#include "nsbench/common_types.h"

#include <string>
#include <vector>

namespace nsbench {

struct DatasetManifest {
    std::string dataset_name;
    uint32_t depth{0};
    uint32_t siblings_per_dir{0};
    uint32_t files_per_leaf{0};
    uint64_t total_records{0};
    uint64_t total_queries{0};
    std::string records_tsv;
    std::string positive_queries_tsv;
    std::string negative_queries_tsv;
};

bool WriteManifest(const DatasetManifest& manifest,
                   const std::string& path,
                   std::string* error);
bool ReadManifest(const std::string& path,
                  DatasetManifest* manifest,
                  std::string* error);

bool WriteNamespaceRecords(const std::vector<NamespaceRecord>& records,
                           const std::string& path,
                           std::string* error);
bool ReadNamespaceRecords(const std::string& path,
                          std::vector<NamespaceRecord>* records,
                          std::string* error);

bool WriteQueries(const std::vector<QueryRecord>& queries,
                  const std::string& path,
                  std::string* error);
bool ReadQueries(const std::string& path,
                 std::vector<QueryRecord>* queries,
                 std::string* error);

}  // namespace nsbench
