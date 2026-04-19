#pragma once

#include "nsbench/resolver.h"

#include <memory>
#include <string>

namespace rocksdb {
class DB;
class WriteBatch;
class Options;
}

namespace nsbench {

struct RocksResolverOptions {
    std::string db_path;
    bool create_if_missing{true};
    bool destroy_if_exists{true};
    bool verify_inode_on_resolve{false};
    uint64_t root_inode_id{1};
};

class RocksIterativeResolver final : public IPathResolver {
public:
    explicit RocksIterativeResolver(RocksResolverOptions options);
    ~RocksIterativeResolver() override;

    const char* Name() const noexcept override;
    bool Build(const std::vector<NamespaceRecord>& records,
               std::string* error) override;
    bool Resolve(const PreparedPath& path,
                 ResolveResult* out,
                 std::string* error) const override;
    bool Warmup(const std::vector<QueryRecord>& queries,
                std::string* error) const override;

private:
    bool Open(std::string* error);
    bool ResetDb(std::string* error);
    bool PutRecord(const NamespaceRecord& record, rocksdb::WriteBatch* batch, std::string* error);
    bool LookupDentry(uint64_t parent_inode_id,
                      const std::string& name,
                      uint64_t* child_inode_id,
                      NodeType* child_type,
                      std::string* error) const;
    bool LookupInode(uint64_t inode_id,
                     NamespaceRecord* record,
                     std::string* error) const;

private:
    RocksResolverOptions options_;
    std::unique_ptr<rocksdb::Options> db_options_;
    std::unique_ptr<rocksdb::DB> db_;
};

}  // namespace nsbench
