#include "fsbench/csv_writer.h"

#include <fstream>

namespace fsbench {

bool WriteMemoryResults(const std::vector<MemoryResultRow>& rows,
                        const std::filesystem::path& path,
                        std::string* error) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) {
        if (error) {
            *error = "failed to open output csv: " + path.string();
        }
        return false;
    }

    out << "backend,file_count,depth,siblings_per_dir,files_per_leaf,phase,"
           "total_meta_bytes,bytes_per_file,slab_dentry_bytes,slab_inode_bytes,"
           "slab_ext4_inode_bytes,lhm_index_bytes,lhm_inode_bytes,lhm_string_bytes,"
           "process_rss_bytes\n";
    for (const MemoryResultRow& row : rows) {
        out << row.backend << ','
            << row.file_count << ','
            << row.depth << ','
            << row.siblings_per_dir << ','
            << row.files_per_leaf << ','
            << row.phase << ','
            << row.snapshot.total_meta_bytes << ','
            << row.snapshot.bytes_per_file << ','
            << row.snapshot.slab_dentry_bytes << ','
            << row.snapshot.slab_inode_bytes << ','
            << row.snapshot.slab_ext4_inode_bytes << ','
            << row.snapshot.lhm_index_bytes << ','
            << row.snapshot.lhm_inode_bytes << ','
            << row.snapshot.lhm_string_bytes << ','
            << row.snapshot.process_rss_bytes << '\n';
    }

    if (error) {
        error->clear();
    }
    return true;
}

bool WriteMissResults(const std::vector<MissResultRow>& rows,
                      const std::filesystem::path& path,
                      std::string* error) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) {
        if (error) {
            *error = "failed to open output csv: " + path.string();
        }
        return false;
    }

    out << "backend,mode,op,query_kind,query_count,file_count,depth,siblings_per_dir,"
           "files_per_leaf,avg_ns,p50_ns,p95_ns,p99_ns,avg_bytes,success_rate\n";
    for (const MissResultRow& row : rows) {
        out << row.backend << ','
            << row.mode << ','
            << row.op << ','
            << row.query_kind << ','
            << row.query_count << ','
            << row.file_count << ','
            << row.depth << ','
            << row.siblings_per_dir << ','
            << row.files_per_leaf << ','
            << row.avg_ns << ','
            << row.p50_ns << ','
            << row.p95_ns << ','
            << row.p99_ns << ','
            << row.avg_bytes << ','
            << row.success_rate << '\n';
    }

    if (error) {
        error->clear();
    }
    return true;
}

}  // namespace fsbench
