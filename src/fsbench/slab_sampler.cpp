#include "fsbench/slab_sampler.h"

#include <fstream>
#include <sstream>

namespace fsbench {

bool SlabSampler::Snapshot(std::vector<SlabEntry>* out, std::string* error) const {
    if (!out) {
        if (error) {
            *error = "slab snapshot output is null";
        }
        return false;
    }

    std::ifstream in("/proc/slabinfo");
    if (!in) {
        if (error) {
            *error = "failed to open /proc/slabinfo";
        }
        return false;
    }

    out->clear();
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (line_no <= 2 || line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        SlabEntry entry;
        if (!(iss >> entry.name >> entry.active_objs >> entry.num_objs >> entry.obj_size)) {
            continue;
        }
        out->push_back(std::move(entry));
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool SlabSampler::QueryActiveBytes(const std::vector<SlabEntry>& slabs,
                                   const std::string& slab_name,
                                   uint64_t* active_bytes,
                                   std::string* error) const {
    if (!active_bytes) {
        if (error) {
            *error = "active bytes output is null";
        }
        return false;
    }

    for (const SlabEntry& entry : slabs) {
        if (entry.name == slab_name) {
            *active_bytes = entry.active_bytes();
            if (error) {
                error->clear();
            }
            return true;
        }
    }

    *active_bytes = 0;
    if (error) {
        error->clear();
    }
    return true;
}

}  // namespace fsbench
