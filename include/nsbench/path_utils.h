#pragma once

#include "nsbench/common_types.h"

#include <string>
#include <vector>

namespace nsbench {

bool NormalizeAbsolutePath(const std::string& input, std::string* normalized);
bool SplitNormalizedPath(const std::string& normalized, std::vector<std::string>* components);
bool PreparePath(const std::string& input, PreparedPath* out, std::string* error);
uint32_t PathDepth(const std::string& normalized);

}  // namespace nsbench
