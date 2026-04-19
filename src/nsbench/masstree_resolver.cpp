#include "nsbench/masstree_resolver.h"

#include "nsbench/masstree_runtime.h"

#include <algorithm>
#include <limits>

namespace nsbench {

MasstreeResolver::MasstreeResolver() = default;

MasstreeResolver::~MasstreeResolver() {
    if (initialized_ && main_ti_ != nullptr) {
        ns_.destroy(*main_ti_);
    }
}

const char* MasstreeResolver::Name() const noexcept {
    return "masstree_lhm";
}

bool MasstreeResolver::Build(const std::vector<NamespaceRecord>& records,
                             std::string* error) {
    if (!EnsureInitialized()) {
        if (error) {
            *error = "failed to initialize masstree runtime";
        }
        return false;
    }

    std::vector<const NamespaceRecord*> ordered;
    ordered.reserve(records.size());
    for (const NamespaceRecord& record : records) {
        ordered.push_back(&record);
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const NamespaceRecord* lhs, const NamespaceRecord* rhs) {
                  const uint32_t lhs_depth = lhs->prepared.depth();
                  const uint32_t rhs_depth = rhs->prepared.depth();
                  if (lhs_depth != rhs_depth) {
                      return lhs_depth < rhs_depth;
                  }
                  if (lhs->type != rhs->type) {
                      return lhs->type == NodeType::kDirectory;
                  }
                  return lhs->path < rhs->path;
              });

    for (const NamespaceRecord* record : ordered) {
        if (record->path == "/") {
            continue;
        }
        if (!InsertOne(*record, error)) {
            return false;
        }
    }

    if (error) {
        error->clear();
    }
    return true;
}

bool MasstreeResolver::Resolve(const PreparedPath& path,
                               ResolveResult* out,
                               std::string* error) const {
    if (!initialized_ || !main_ti_ || !out) {
        if (error) {
            *error = "masstree resolver is not initialized";
        }
        return false;
    }

    MasstreeLHM::ParsedPath parsed;
    parsed.normalized_path = path.normalized;
    parsed.components = path.components;
    parsed.hashes = path.masstree_hashes;

    MasstreeLHM::inode_ref ref;
    MasstreeLHM::lookup_probe_stats lookup_steps;
    const bool found = ns_.lookup_inode_from_parsed(parsed, ref, *main_ti_, &lookup_steps);

    ResolveResult result;
    result.found = found;
    result.inode_id = found ? ref.block_id : 0;
    result.depth = path.depth();
    result.component_steps = lookup_steps.directory_levels_walked + lookup_steps.child_slot_lookups;
    result.index_steps = lookup_steps.directory_levels_walked + lookup_steps.child_slot_lookups;
    result.steps = result.index_steps;
    *out = result;

    if (error) {
        error->clear();
    }
    return true;
}

bool MasstreeResolver::Warmup(const std::vector<QueryRecord>& queries,
                              std::string* error) const {
    for (const QueryRecord& query : queries) {
        ResolveResult result;
        if (!Resolve(query.prepared, &result, error)) {
            return false;
        }
    }
    return true;
}

bool MasstreeResolver::EnsureInitialized() {
    if (initialized_) {
        return true;
    }
    main_ti_ = CreateMainThreadInfo();
    if (!main_ti_) {
        return false;
    }
    ns_.initialize(*main_ti_);
    initialized_ = true;
    return true;
}

bool MasstreeResolver::InsertOne(const NamespaceRecord& record, std::string* error) {
    if (record.inode_id > std::numeric_limits<uint32_t>::max()) {
        if (error) {
            *error = "inode id exceeds uint32_t range for masstree inode_ref";
        }
        return false;
    }

    const MasstreeLHM::inode_ref ref =
        MasstreeLHM::make_inode_ref(static_cast<uint32_t>(record.inode_id), 0);
    const bool ok = (record.type == NodeType::kDirectory)
                        ? ns_.mkdir(record.path, ref, *main_ti_)
                        : ns_.creat_file(record.path, ref, *main_ti_);
    if (!ok && error) {
        *error = "failed to insert path into masstree: " + record.path;
    }
    return ok;
}

}  // namespace nsbench
