#include "nsbench/path_utils.h"

#include <algorithm>
#include <sstream>

namespace nsbench {

namespace {

uint64_t HashPathComponent(const std::string& component) {
    static const uint64_t kFnvOffset = 14695981039346656037ull;
    static const uint64_t kFnvPrime = 1099511628211ull;

    uint64_t hash = kFnvOffset;
    for (size_t i = 0; i < component.size(); ++i) {
        hash ^= static_cast<unsigned char>(component[i]);
        hash *= kFnvPrime;
    }
    return hash;
}

}  // namespace

bool NormalizeAbsolutePath(const std::string& input, std::string* normalized) {
    if (!normalized || input.empty()) {
        return false;
    }

    std::string path(input);
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.empty() || path.front() != '/') {
        return false;
    }

    std::string out;
    out.reserve(path.size());
    bool prev_slash = false;
    for (char ch : path) {
        if (ch == '/') {
            if (prev_slash) {
                continue;
            }
            prev_slash = true;
            out.push_back(ch);
            continue;
        }
        prev_slash = false;
        out.push_back(ch);
    }
    while (out.size() > 1 && out.back() == '/') {
        out.pop_back();
    }
    if (out.empty()) {
        out = "/";
    }
    *normalized = std::move(out);
    return true;
}

bool SplitNormalizedPath(const std::string& normalized, std::vector<std::string>* components) {
    if (!components || normalized.empty() || normalized.front() != '/') {
        return false;
    }
    components->clear();
    if (normalized == "/") {
        return true;
    }

    std::string token;
    std::istringstream stream(normalized);
    while (std::getline(stream, token, '/')) {
        if (!token.empty()) {
            components->push_back(token);
        }
    }
    return true;
}

bool PreparePath(const std::string& input, PreparedPath* out, std::string* error) {
    if (!out) {
        if (error) {
            *error = "prepared path output is null";
        }
        return false;
    }

    std::string normalized;
    if (!NormalizeAbsolutePath(input, &normalized)) {
        if (error) {
            *error = "invalid absolute path";
        }
        return false;
    }

    std::vector<std::string> components;
    if (!SplitNormalizedPath(normalized, &components)) {
        if (error) {
            *error = "failed to split normalized path";
        }
        return false;
    }

    PreparedPath prepared;
    prepared.normalized = std::move(normalized);
    prepared.components = std::move(components);
    prepared.masstree_hashes.reserve(prepared.components.size());
    for (const std::string& component : prepared.components) {
        prepared.masstree_hashes.push_back(HashPathComponent(component));
    }

    *out = std::move(prepared);
    if (error) {
        error->clear();
    }
    return true;
}

uint32_t PathDepth(const std::string& normalized) {
    if (normalized.empty() || normalized == "/") {
        return 0;
    }
    uint32_t depth = 0;
    bool in_component = false;
    for (char ch : normalized) {
        if (ch == '/') {
            in_component = false;
            continue;
        }
        if (!in_component) {
            ++depth;
            in_component = true;
        }
    }
    return depth;
}

}  // namespace nsbench
