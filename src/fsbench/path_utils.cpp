#include "fsbench/path_utils.h"

#include <algorithm>

namespace fsbench {

bool NormalizeAbsolutePath(std::string_view input, std::string* normalized) {
    if (!normalized) {
        return false;
    }
    if (input.empty() || input.front() != '/') {
        return false;
    }

    std::string out;
    out.reserve(input.size());
    bool last_was_slash = false;
    for (char ch : input) {
        if (ch == '\\') {
            ch = '/';
        }
        if (ch == '/') {
            if (last_was_slash) {
                continue;
            }
            last_was_slash = true;
        } else {
            last_was_slash = false;
        }
        out.push_back(ch);
    }

    if (out.empty()) {
        out = "/";
    }
    if (out.size() > 1 && out.back() == '/') {
        out.pop_back();
    }
    *normalized = std::move(out);
    return true;
}

bool SplitNormalizedPath(std::string_view normalized, std::vector<std::string>* components) {
    if (!components || normalized.empty() || normalized.front() != '/') {
        return false;
    }
    components->clear();
    if (normalized == "/") {
        return true;
    }

    size_t start = 1;
    while (start < normalized.size()) {
        const size_t slash = normalized.find('/', start);
        if (slash == std::string_view::npos) {
            components->emplace_back(normalized.substr(start));
            break;
        }
        components->emplace_back(normalized.substr(start, slash - start));
        start = slash + 1;
    }
    return true;
}

bool PreparePath(std::string_view input, PreparedPath* out, std::string* error) {
    if (!out) {
        if (error) {
            *error = "prepared path output is null";
        }
        return false;
    }
    std::string normalized;
    if (!NormalizeAbsolutePath(input, &normalized)) {
        if (error) {
            *error = "path must be absolute";
        }
        return false;
    }

    PreparedPath prepared;
    prepared.normalized = std::move(normalized);
    if (!SplitNormalizedPath(prepared.normalized, &prepared.components)) {
        if (error) {
            *error = "failed to split normalized path";
        }
        return false;
    }
    *out = std::move(prepared);
    if (error) {
        error->clear();
    }
    return true;
}

}  // namespace fsbench
