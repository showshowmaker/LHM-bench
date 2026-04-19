#include "nsbench/rocks_schema.h"

#include "nsbench/path_utils.h"

#include <cstring>

namespace nsbench {

namespace {

void AppendU64(std::string* out, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out->push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

bool ReadU64(const std::string& input, size_t offset, uint64_t* value) {
    if (!value || offset + sizeof(uint64_t) > input.size()) {
        return false;
    }
    uint64_t out = 0;
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
        out = (out << 8U) | static_cast<unsigned char>(input[offset + i]);
    }
    *value = out;
    return true;
}

}  // namespace

std::string EncodeInodeKey(uint64_t inode_id) {
    std::string key("I", 1);
    AppendU64(&key, inode_id);
    return key;
}

std::string EncodeDentryKey(uint64_t parent_inode_id, const std::string& name) {
    std::string key("D", 1);
    AppendU64(&key, parent_inode_id);
    key.append(name.data(), name.size());
    return key;
}

std::string EncodeInodeValue(const NamespaceRecord& record) {
    std::string value;
    value.reserve(18 + record.path.size());
    AppendU64(&value, record.inode_id);
    AppendU64(&value, record.parent_inode_id);
    value.push_back(static_cast<char>(record.type));
    value.append(record.path);
    return value;
}

bool DecodeInodeValue(const std::string& value, NamespaceRecord* record) {
    if (!record || value.size() < 17) {
        return false;
    }
    NamespaceRecord parsed;
    if (!ReadU64(value, 0, &parsed.inode_id) ||
        !ReadU64(value, 8, &parsed.parent_inode_id)) {
        return false;
    }
    parsed.type = static_cast<NodeType>(static_cast<uint8_t>(value[16]));
    parsed.path.assign(value.substr(17));
    std::string error;
    if (!PreparePath(parsed.path, &parsed.prepared, &error)) {
        return false;
    }
    *record = std::move(parsed);
    return true;
}

std::string EncodeDentryValue(uint64_t child_inode_id, NodeType child_type) {
    std::string value;
    value.reserve(9);
    AppendU64(&value, child_inode_id);
    value.push_back(static_cast<char>(child_type));
    return value;
}

bool DecodeDentryValue(const std::string& value, RocksDentryValue* out) {
    if (!out || value.size() != 9) {
        return false;
    }
    RocksDentryValue parsed;
    if (!ReadU64(value, 0, &parsed.child_inode_id)) {
        return false;
    }
    parsed.child_type = static_cast<NodeType>(static_cast<uint8_t>(value[8]));
    *out = parsed;
    return true;
}

}  // namespace nsbench
