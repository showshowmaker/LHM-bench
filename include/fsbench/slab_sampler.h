#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fsbench {

struct SlabEntry {
    std::string name;
    uint64_t active_objs = 0;
    uint64_t num_objs = 0;
    uint64_t obj_size = 0;

    uint64_t active_bytes() const noexcept {
        return active_objs * obj_size;
    }
};

class SlabSampler {
public:
    bool Snapshot(std::vector<SlabEntry>* out, std::string* error) const;
    bool QueryActiveBytes(const std::vector<SlabEntry>& slabs,
                          const std::string& slab_name,
                          uint64_t* active_bytes,
                          std::string* error) const;
};

}  // namespace fsbench
