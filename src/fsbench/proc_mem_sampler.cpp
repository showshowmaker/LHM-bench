#include "fsbench/proc_mem_sampler.h"

#include <fstream>
#include <sstream>

namespace fsbench {
namespace {

uint64_t ParseKbLine(const std::string& line) {
    std::istringstream iss(line);
    std::string key;
    uint64_t value = 0;
    std::string unit;
    iss >> key >> value >> unit;
    return value * 1024;
}

}  // namespace

bool ProcMemSampler::SnapshotSelf(ProcMemInfo* out, std::string* error) const {
    if (!out) {
        if (error) {
            *error = "proc mem output is null";
        }
        return false;
    }

    std::ifstream in("/proc/self/status");
    if (!in) {
        if (error) {
            *error = "failed to open /proc/self/status";
        }
        return false;
    }

    ProcMemInfo info;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            info.rss_bytes = ParseKbLine(line);
        } else if (line.rfind("VmSize:", 0) == 0) {
            info.vm_bytes = ParseKbLine(line);
        }
    }
    *out = info;
    if (error) {
        error->clear();
    }
    return true;
}

}  // namespace fsbench
