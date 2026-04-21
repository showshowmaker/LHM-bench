#pragma once

#include "fsbench/common_types.h"

#include <string>
#include <string_view>

namespace fsbench {

bool NormalizeAbsolutePath(std::string_view input, std::string* normalized);
bool SplitNormalizedPath(std::string_view normalized, std::vector<std::string>* components);
bool PreparePath(std::string_view input, PreparedPath* out, std::string* error);

}  // namespace fsbench
