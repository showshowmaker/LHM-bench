#pragma once

#include <string>

namespace fsbench {

class CacheController {
public:
    bool Sync(std::string* error) const;
    bool DropPageCache(std::string* error) const;
    bool DropAllCaches(std::string* error) const;
};

}  // namespace fsbench
