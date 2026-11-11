#pragma once

#include <memory>
#include <string>

#include "Dictionary/IDictionary.hpp"
#include "FSNode.hpp"
#include "VirtualFileSystem/FileNameIndex.hpp"

using NodeDictionary = IDictionary<NodeId, FSNode>;

class VirtualFileSystem {
private:
    std::unique_ptr<NodeDictionary> nodes_;
    FileNameIndex file_index_;

    NodeId root_id_ = kInvalidNodeId;
    NodeId next_id_ = 1;
public:
    explicit VirtualFileSystem(
        std::unique_ptr<NodeDictionary> nodes, std::unique_ptr<FileIndexDictionary> index_dictionary);

    NodeId root_id() const noexcept;

    const FSNode& get_node(NodeId id) const;

    NodeId create_directory(NodeId parent_id, const std::string& name);

    NodeId attach_file(NodeId parent_id, const std::string& name, const std::string& physical_path);

    ArraySequence<NodeId> find_files(
    const std::string& name) const;

    void rename(NodeId id, const std::string& new_name);
};