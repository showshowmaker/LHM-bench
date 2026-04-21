#pragma once

#include <cstdint>
#include <string>

namespace fsbench {

struct ProcMemInfo {
    uint64_t rss_bytes = 0;
    uint64_t vm_bytes = 0;
};

class ProcMemSampler {
public:
    bool SnapshotSelf(ProcMemInfo* out, std::string* error) const;
};

}  // namespace fsbench
