#include "flatbench/flatlike_backend.h"

namespace flatbench {

FlatLikeBackend::FlatLikeBackend(const FlatLikeOptions& options)
    : tree_(options.leaf_capacity) {
}

const char* FlatLikeBackend::Name() const noexcept {
    return "flatfs_like";
}

bool FlatLikeBackend::Build(const std::vector<nsbench::NamespaceRecord>& records,
                            std::string* error) {
    return tree_.Build(records, error);
}

bool FlatLikeBackend::Lookup(const nsbench::PreparedPath& path,
                             OpStats* out,
                             std::string* error) const {
    return tree_.Lookup(path, out, error);
}

bool FlatLikeBackend::Insert(const nsbench::NamespaceRecord& record,
                             OpStats* out,
                             std::string* error) {
    return tree_.Insert(record, out, error);
}

bool FlatLikeBackend::Erase(const nsbench::PreparedPath& path,
                            OpStats* out,
                            std::string* error) {
    return tree_.Erase(path, out, error);
}

bool FlatLikeBackend::RenameSubtree(const nsbench::PreparedPath& src,
                                    const nsbench::PreparedPath& dst,
                                    OpStats* out,
                                    std::string* error) {
    return tree_.RenameSubtree(src, dst, out, error);
}

MemoryStats FlatLikeBackend::SnapshotMemory() const {
    return tree_.SnapshotMemory();
}

FlatInternalCounters FlatLikeBackend::Counters() const {
    return tree_.Counters();
}

void FlatLikeBackend::ResetCounters() {
    tree_.ResetCounters();
}

size_t FlatLikeBackend::leaf_count() const {
    return tree_.leaf_count();
}

size_t FlatLikeBackend::entry_count() const {
    return tree_.entry_count();
}

}  // namespace flatbench
