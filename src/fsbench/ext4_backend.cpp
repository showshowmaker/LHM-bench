#include "fsbench/ext4_backend.h"

#include "fsbench/proc_mem_sampler.h"
#include "fsbench/slab_sampler.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fsbench {
namespace {

uint64_t NowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace

Ext4Backend::Ext4Backend(Ext4BackendOptions options)
    : options_(std::move(options)) {
}

const char* Ext4Backend::Name() const noexcept {
    return "ext4";
}

bool Ext4Backend::Build(const std::vector<NamespaceEntry>& entries,
                        std::string* error) {
    if (options_.mount_root.empty()) {
        if (error) {
            *error = "ext4 mount root is empty";
        }
        return false;
    }
    std::filesystem::create_directories(options_.mount_root);
    for (const NamespaceEntry& entry : entries) {
        if (!BuildOne(entry, error)) {
            return false;
        }
    }
    return true;
}

bool Ext4Backend::Run(const PreparedPath& path,
                      OpKind op,
                      OpResult* out,
                      std::string* error) {
    switch (op) {
    case OpKind::kLookupOnly:
        return LookupOnly(path, out, error);
    case OpKind::kOpenRead4K:
        return OpenRead4K(path, out, error);
    case OpKind::kOpenWrite4K:
        return OpenWrite4K(path, out, error);
    case OpKind::kNegativeLookup:
        return LookupOnly(path, out, error);
    default:
        if (error) {
            *error = "unsupported ext4 op";
        }
        return false;
    }
}

bool Ext4Backend::SnapshotMemory(uint64_t file_count,
                                 MemorySnapshot* out,
                                 std::string* error) const {
    if (!out) {
        if (error) {
            *error = "memory snapshot output is null";
        }
        return false;
    }

    SlabSampler slab_sampler;
    std::vector<SlabEntry> slabs;
    if (!slab_sampler.Snapshot(&slabs, error)) {
        return false;
    }

    MemorySnapshot snapshot;
    slab_sampler.QueryActiveBytes(slabs, "dentry", &snapshot.slab_dentry_bytes, nullptr);
    slab_sampler.QueryActiveBytes(slabs, "inode_cache", &snapshot.slab_inode_bytes, nullptr);
    slab_sampler.QueryActiveBytes(slabs, "ext4_inode_cache", &snapshot.slab_ext4_inode_bytes, nullptr);
    snapshot.total_meta_bytes = snapshot.slab_dentry_bytes + snapshot.slab_inode_bytes +
                                snapshot.slab_ext4_inode_bytes;
    snapshot.bytes_per_file = file_count == 0 ? 0 : snapshot.total_meta_bytes / file_count;

    ProcMemInfo mem_info;
    ProcMemSampler proc_mem_sampler;
    if (proc_mem_sampler.SnapshotSelf(&mem_info, nullptr)) {
        snapshot.process_rss_bytes = mem_info.rss_bytes;
    }

    *out = snapshot;
    if (error) {
        error->clear();
    }
    return true;
}

bool Ext4Backend::BuildOne(const NamespaceEntry& entry, std::string* error) {
    const std::string host_path = ToHostPath(entry.prepared);
    try {
        if (entry.type == NodeType::kDirectory) {
            std::filesystem::create_directories(host_path);
        } else {
            std::filesystem::create_directories(std::filesystem::path(host_path).parent_path());
            std::ofstream out(host_path.c_str(), std::ios::binary | std::ios::app);
            if (!out) {
                if (error) {
                    *error = "failed to create file: " + host_path;
                }
                return false;
            }
        }
    } catch (const std::exception& ex) {
        if (error) {
            *error = ex.what();
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool Ext4Backend::LookupOnly(const PreparedPath& path, OpResult* out, std::string* error) {
#ifdef _WIN32
    (void) path;
    (void) out;
    if (error) {
        *error = "ext4 backend is not available on Windows";
    }
    return false;
#else
    struct stat st;
    const uint64_t begin = NowNs();
    const int rc = stat(ToHostPath(path).c_str(), &st);
    const uint64_t end = NowNs();

    if (out) {
        out->ok = (rc == 0);
        out->latency_ns = end - begin;
        out->bytes = 0;
        out->depth = path.depth();
    }
    if (rc != 0 && error) {
        *error = "stat failed";
    } else if (error) {
        error->clear();
    }
    return true;
#endif
}

bool Ext4Backend::OpenRead4K(const PreparedPath& path, OpResult* out, std::string* error) {
#ifdef _WIN32
    (void) path;
    (void) out;
    if (error) {
        *error = "ext4 backend is not available on Windows";
    }
    return false;
#else
    char buffer[4096] = {0};
    const uint64_t begin = NowNs();
    const int fd = open(ToHostPath(path).c_str(), O_RDONLY);
    ssize_t n = -1;
    if (fd >= 0) {
        n = pread(fd, buffer, sizeof(buffer), 0);
        close(fd);
    }
    const uint64_t end = NowNs();
    if (out) {
        out->ok = (fd >= 0 && n >= 0);
        out->latency_ns = end - begin;
        out->bytes = n > 0 ? static_cast<uint64_t>(n) : 0;
        out->depth = path.depth();
    }
    if ((fd < 0 || n < 0) && error) {
        *error = "open/pread failed";
    } else if (error) {
        error->clear();
    }
    return true;
#endif
}

bool Ext4Backend::OpenWrite4K(const PreparedPath& path, OpResult* out, std::string* error) {
#ifdef _WIN32
    (void) path;
    (void) out;
    if (error) {
        *error = "ext4 backend is not available on Windows";
    }
    return false;
#else
    char buffer[4096] = {0};
    const uint64_t begin = NowNs();
    const int fd = open(ToHostPath(path).c_str(), O_WRONLY);
    ssize_t n = -1;
    if (fd >= 0) {
        n = pwrite(fd, buffer, sizeof(buffer), 0);
        if (options_.fsync_writes) {
            fsync(fd);
        }
        close(fd);
    }
    const uint64_t end = NowNs();
    if (out) {
        out->ok = (fd >= 0 && n >= 0);
        out->latency_ns = end - begin;
        out->bytes = n > 0 ? static_cast<uint64_t>(n) : 0;
        out->depth = path.depth();
    }
    if ((fd < 0 || n < 0) && error) {
        *error = "open/pwrite failed";
    } else if (error) {
        error->clear();
    }
    return true;
#endif
}

std::string Ext4Backend::ToHostPath(const PreparedPath& path) const {
    if (path.normalized == "/") {
        return options_.mount_root;
    }
    if (!options_.mount_root.empty() &&
        (options_.mount_root.back() == '/' || options_.mount_root.back() == '\\')) {
        return options_.mount_root + path.normalized.substr(1);
    }
    return options_.mount_root + path.normalized;
}

}  // namespace fsbench
