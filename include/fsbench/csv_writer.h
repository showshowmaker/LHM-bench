#pragma once

#include "fsbench/common_types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fsbench {

struct MemoryResultRow {
    std::string backend;
    uint64_t file_count = 0;
    uint32_t depth = 0;
    uint32_t siblings_per_dir = 0;
    uint32_t files_per_leaf = 0;
    std::string phase;
    MemorySnapshot snapshot;
};

struct MissResultRow {
    std::string backend;
    std::string mode;
    std::string op;
    std::string query_kind;
    uint64_t query_count = 0;
    uint64_t file_count = 0;
    uint32_t depth = 0;
    uint32_t siblings_per_dir = 0;
    uint32_t files_per_leaf = 0;
    double avg_ns = 0.0;
    double p50_ns = 0.0;
    double p95_ns = 0.0;
    double p99_ns = 0.0;
    double avg_bytes = 0.0;
    double success_rate = 0.0;
};

bool WriteMemoryResults(const std::vector<MemoryResultRow>& rows,
                        const std::filesystem::path& path,
                        std::string* error);

bool WriteMissResults(const std::vector<MissResultRow>& rows,
                      const std::filesystem::path& path,
                      std::string* error);

}  // namespace fsbench
