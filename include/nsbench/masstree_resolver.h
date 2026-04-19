#pragma once

#include "nsbench/resolver.h"

#include "lhm_namespace.hh"

namespace nsbench {

class MasstreeResolver final : public IPathResolver {
public:
    MasstreeResolver();
    ~MasstreeResolver() override;

    const char* Name() const noexcept override;
    bool Build(const std::vector<NamespaceRecord>& records,
               std::string* error) override;
    bool Resolve(const PreparedPath& path,
                 ResolveResult* out,
                 std::string* error) const override;
    bool Warmup(const std::vector<QueryRecord>& queries,
                std::string* error) const override;

private:
    bool EnsureInitialized();
    bool InsertOne(const NamespaceRecord& record, std::string* error);

private:
    bool initialized_{false};
    threadinfo* main_ti_{nullptr};
    MasstreeLHM::LhmNamespace ns_;
};

}  // namespace nsbench
