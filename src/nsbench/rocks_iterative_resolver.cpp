#include "nsbench/rocks_iterative_resolver.h"

#include "nsbench/rocks_schema.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>

namespace nsbench {

RocksIterativeResolver::RocksIterativeResolver(RocksResolverOptions options)
    : options_(std::move(options)),
      db_options_(std::make_unique<rocksdb::Options>()) {
    db_options_->create_if_missing = options_.create_if_missing;
}

RocksIterativeResolver::~RocksIterativeResolver() = default;

const char* RocksIterativeResolver::Name() const noexcept {
    return "rocksdb_iterative";
}

bool RocksIterativeResolver::Build(const std::vector<NamespaceRecord>& records,
                                   std::string* error) {
    if (!ResetDb(error) || !Open(error)) {
        return false;
    }

    rocksdb::WriteBatch batch;
    for (const NamespaceRecord& record : records) {
        if (!PutRecord(record, &batch, error)) {
            return false;
        }
    }

    const rocksdb::Status status = db_->Write(rocksdb::WriteOptions(), &batch);
    if (!status.ok()) {
        if (error) {
            *error = status.ToString();
        }
        return false;
    }

    if (error) {
        error->clear();
    }
    return true;
}

bool RocksIterativeResolver::Resolve(const PreparedPath& path,
                                     ResolveResult* out,
                                     std::string* error) const {
    if (!db_ || !out) {
        if (error) {
            *error = "rocksdb resolver is not ready";
        }
        return false;
    }

    uint64_t current = options_.root_inode_id;
    uint32_t component_steps = 0;
    uint32_t index_steps = 0;
    for (const std::string& component : path.components) {
        uint64_t child_inode = 0;
        NodeType child_type = NodeType::kFile;
        ++index_steps;
        if (!LookupDentry(current, component, &child_inode, &child_type, error)) {
            ResolveResult miss;
            miss.found = false;
            miss.inode_id = 0;
            miss.depth = path.depth();
            miss.component_steps = component_steps;
            miss.index_steps = index_steps;
            miss.steps = index_steps;
            *out = miss;
            if (error) {
                error->clear();
            }
            return true;
        }
        ++component_steps;
        current = child_inode;
    }

    if (options_.verify_inode_on_resolve) {
        NamespaceRecord inode_record;
        ++index_steps;
        if (!LookupInode(current, &inode_record, error)) {
            return false;
        }
    }

    ResolveResult result;
    result.found = true;
    result.inode_id = current;
    result.depth = path.depth();
    result.component_steps = component_steps;
    result.index_steps = index_steps;
    result.steps = index_steps;
    *out = result;

    if (error) {
        error->clear();
    }
    return true;
}

bool RocksIterativeResolver::Warmup(const std::vector<QueryRecord>& queries,
                                    std::string* error) const {
    for (const QueryRecord& query : queries) {
        ResolveResult result;
        if (!Resolve(query.prepared, &result, error)) {
            return false;
        }
    }
    return true;
}

bool RocksIterativeResolver::Open(std::string* error) {
    if (db_) {
        return true;
    }

    std::unique_ptr<rocksdb::DB> db;
    const rocksdb::Status status = rocksdb::DB::Open(*db_options_, options_.db_path, &db);
    if (!status.ok()) {
        if (error) {
            *error = status.ToString();
        }
        return false;
    }
    db_ = std::move(db);
    return true;
}

bool RocksIterativeResolver::ResetDb(std::string* error) {
    db_.reset();
    if (!options_.destroy_if_exists) {
        return true;
    }

    const rocksdb::Status status = rocksdb::DestroyDB(options_.db_path, *db_options_);
    if (!status.ok() && !status.IsNotFound()) {
        if (error) {
            *error = status.ToString();
        }
        return false;
    }
    return true;
}

bool RocksIterativeResolver::PutRecord(const NamespaceRecord& record,
                                       rocksdb::WriteBatch* batch,
                                       std::string* error) {
    if (!batch) {
        if (error) {
            *error = "rocksdb write batch is null";
        }
        return false;
    }

    batch->Put(EncodeInodeKey(record.inode_id), EncodeInodeValue(record));
    if (record.path != "/") {
        batch->Put(EncodeDentryKey(record.parent_inode_id, record.prepared.components.back()),
                   EncodeDentryValue(record.inode_id, record.type));
    }
    return true;
}

bool RocksIterativeResolver::LookupDentry(uint64_t parent_inode_id,
                                          const std::string& name,
                                          uint64_t* child_inode_id,
                                          NodeType* child_type,
                                          std::string* error) const {
    std::string value;
    const rocksdb::Status status =
        db_->Get(rocksdb::ReadOptions(), EncodeDentryKey(parent_inode_id, name), &value);
    if (status.IsNotFound()) {
        return false;
    }
    if (!status.ok()) {
        if (error) {
            *error = status.ToString();
        }
        return false;
    }

    RocksDentryValue decoded;
    if (!DecodeDentryValue(value, &decoded)) {
        if (error) {
            *error = "invalid rocksdb dentry payload";
        }
        return false;
    }
    if (child_inode_id) {
        *child_inode_id = decoded.child_inode_id;
    }
    if (child_type) {
        *child_type = decoded.child_type;
    }
    return true;
}

bool RocksIterativeResolver::LookupInode(uint64_t inode_id,
                                         NamespaceRecord* record,
                                         std::string* error) const {
    std::string value;
    const rocksdb::Status status =
        db_->Get(rocksdb::ReadOptions(), EncodeInodeKey(inode_id), &value);
    if (!status.ok()) {
        if (error) {
            *error = status.ToString();
        }
        return false;
    }

    if (!DecodeInodeValue(value, record)) {
        if (error) {
            *error = "invalid rocksdb inode payload";
        }
        return false;
    }
    return true;
}

}  // namespace nsbench
