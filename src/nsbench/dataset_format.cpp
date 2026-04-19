#include "nsbench/dataset_format.h"

#include "nsbench/path_utils.h"

#include <fstream>
#include <sstream>

namespace nsbench {

namespace {

bool WriteTextFile(const std::string& path,
                   const std::string& content,
                   std::string* error) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        if (error) {
            *error = "failed to open output file: " + path;
        }
        return false;
    }
    out << content;
    if (!out.good()) {
        if (error) {
            *error = "failed to write output file: " + path;
        }
        return false;
    }
    return true;
}

}  // namespace

bool WriteManifest(const DatasetManifest& manifest,
                   const std::string& path,
                   std::string* error) {
    std::ostringstream out;
    out << "dataset_name=" << manifest.dataset_name << "\n";
    out << "depth=" << manifest.depth << "\n";
    out << "siblings_per_dir=" << manifest.siblings_per_dir << "\n";
    out << "files_per_leaf=" << manifest.files_per_leaf << "\n";
    out << "total_records=" << manifest.total_records << "\n";
    out << "total_queries=" << manifest.total_queries << "\n";
    out << "records_tsv=" << manifest.records_tsv << "\n";
    out << "positive_queries_tsv=" << manifest.positive_queries_tsv << "\n";
    out << "negative_queries_tsv=" << manifest.negative_queries_tsv << "\n";
    return WriteTextFile(path, out.str(), error);
}

bool ReadManifest(const std::string& path,
                  DatasetManifest* manifest,
                  std::string* error) {
    if (!manifest) {
        if (error) {
            *error = "manifest output is null";
        }
        return false;
    }

    std::ifstream in(path);
    if (!in) {
        if (error) {
            *error = "failed to open manifest: " + path;
        }
        return false;
    }

    DatasetManifest parsed;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, pos);
        const std::string value = line.substr(pos + 1);
        if (key == "dataset_name") {
            parsed.dataset_name = value;
        } else if (key == "depth") {
            parsed.depth = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "siblings_per_dir") {
            parsed.siblings_per_dir = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "files_per_leaf") {
            parsed.files_per_leaf = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "total_records") {
            parsed.total_records = std::stoull(value);
        } else if (key == "total_queries") {
            parsed.total_queries = std::stoull(value);
        } else if (key == "records_tsv") {
            parsed.records_tsv = value;
        } else if (key == "positive_queries_tsv") {
            parsed.positive_queries_tsv = value;
        } else if (key == "negative_queries_tsv") {
            parsed.negative_queries_tsv = value;
        }
    }
    *manifest = std::move(parsed);
    if (error) {
        error->clear();
    }
    return true;
}

bool WriteNamespaceRecords(const std::vector<NamespaceRecord>& records,
                           const std::string& path,
                           std::string* error) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        if (error) {
            *error = "failed to open records file: " + path;
        }
        return false;
    }
    out << "inode_id\tparent_inode_id\ttype\tpath\n";
    for (const NamespaceRecord& record : records) {
        out << record.inode_id << '\t'
            << record.parent_inode_id << '\t'
            << NodeTypeName(record.type) << '\t'
            << record.path << '\n';
    }
    if (!out.good()) {
        if (error) {
            *error = "failed to write records file: " + path;
        }
        return false;
    }
    return true;
}

bool ReadNamespaceRecords(const std::string& path,
                          std::vector<NamespaceRecord>* records,
                          std::string* error) {
    if (!records) {
        if (error) {
            *error = "records output is null";
        }
        return false;
    }

    std::ifstream in(path);
    if (!in) {
        if (error) {
            *error = "failed to open records file: " + path;
        }
        return false;
    }

    records->clear();
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) {
            first = false;
            continue;
        }
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        std::string inode_id;
        std::string parent_inode_id;
        std::string type;
        std::string path_text;
        if (!std::getline(row, inode_id, '\t') ||
            !std::getline(row, parent_inode_id, '\t') ||
            !std::getline(row, type, '\t') ||
            !std::getline(row, path_text)) {
            continue;
        }
        NamespaceRecord record;
        record.inode_id = std::stoull(inode_id);
        record.parent_inode_id = std::stoull(parent_inode_id);
        record.type = (type == "dir") ? NodeType::kDirectory : NodeType::kFile;
        record.path = path_text;
        if (!PreparePath(record.path, &record.prepared, error)) {
            return false;
        }
        records->push_back(std::move(record));
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool WriteQueries(const std::vector<QueryRecord>& queries,
                  const std::string& path,
                  std::string* error) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        if (error) {
            *error = "failed to open queries file: " + path;
        }
        return false;
    }
    out << "path\texpect_found\texpected_inode_id\texpected_depth\n";
    for (const QueryRecord& query : queries) {
        out << query.path << '\t'
            << (query.expect_found ? 1 : 0) << '\t'
            << query.expected_inode_id << '\t'
            << query.expected_depth << '\n';
    }
    if (!out.good()) {
        if (error) {
            *error = "failed to write queries file: " + path;
        }
        return false;
    }
    return true;
}

bool ReadQueries(const std::string& path,
                 std::vector<QueryRecord>* queries,
                 std::string* error) {
    if (!queries) {
        if (error) {
            *error = "queries output is null";
        }
        return false;
    }

    std::ifstream in(path);
    if (!in) {
        if (error) {
            *error = "failed to open queries file: " + path;
        }
        return false;
    }

    queries->clear();
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) {
            first = false;
            continue;
        }
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        std::string path_text;
        std::string expect_found;
        std::string expected_inode_id;
        std::string expected_depth;
        if (!std::getline(row, path_text, '\t') ||
            !std::getline(row, expect_found, '\t') ||
            !std::getline(row, expected_inode_id, '\t') ||
            !std::getline(row, expected_depth)) {
            continue;
        }
        QueryRecord query;
        query.path = path_text;
        query.expect_found = (expect_found == "1");
        query.expected_inode_id = std::stoull(expected_inode_id);
        query.expected_depth = static_cast<uint32_t>(std::stoul(expected_depth));
        if (!PreparePath(query.path, &query.prepared, error)) {
            return false;
        }
        queries->push_back(std::move(query));
    }
    if (error) {
        error->clear();
    }
    return true;
}

}  // namespace nsbench
