#include "fsbench/cache_control.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <fstream>

namespace fsbench {

bool CacheController::Sync(std::string* error) const {
#ifdef _WIN32
    if (error) {
        *error = "cache control is not available on Windows";
    }
    return false;
#else
    ::sync();
    if (error) {
        error->clear();
    }
    return true;
#endif
}

bool CacheController::DropPageCache(std::string* error) const {
#ifdef _WIN32
    if (error) {
        *error = "cache control is not available on Windows";
    }
    return false;
#else
    if (!Sync(error)) {
        return false;
    }
    std::ofstream out("/proc/sys/vm/drop_caches");
    if (!out) {
        if (error) {
            *error = "failed to open /proc/sys/vm/drop_caches";
        }
        return false;
    }
    out << "1\n";
    if (!out) {
        if (error) {
            *error = "failed to write drop_caches=1";
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
#endif
}

bool CacheController::DropAllCaches(std::string* error) const {
#ifdef _WIN32
    if (error) {
        *error = "cache control is not available on Windows";
    }
    return false;
#else
    if (!Sync(error)) {
        return false;
    }
    std::ofstream out("/proc/sys/vm/drop_caches");
    if (!out) {
        if (error) {
            *error = "failed to open /proc/sys/vm/drop_caches";
        }
        return false;
    }
    out << "3\n";
    if (!out) {
        if (error) {
            *error = "failed to write drop_caches=3";
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
#endif
}

}  // namespace fsbench
