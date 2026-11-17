#pragma once

#include <memory>
#include <string>
#include <iosfwd>

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

    void require_write(NodeId id, const User& user) const;
    void require_read(NodeId id, const User& user) const;

    const FSNode& get_node_unchecked(NodeId id) const;
public:
    explicit VirtualFileSystem(
        std::unique_ptr<NodeDictionary> nodes, std::unique_ptr<FileIndexDictionary> index_dictionary);

    NodeId root_id() const noexcept;

    const FSNode& get_node(NodeId id, const User& user) const;
    AccessRights get_access_rights(NodeId id, const User& user) const;

    void set_access_policy(NodeId id, const AccessPolicy& policy, const User& user);
    
    NodeId create_directory(NodeId parent_id, const std::string& name, const User& user);
    NodeId attach_file(NodeId parent_id, const std::string& name, const std::string& physical_path, const User& user);

    ArraySequence<NodeId> find_files(
    const std::string& name, const User& user) const;

    void rename(NodeId id, const std::string& new_name, const User& user);

    void move(NodeId id, NodeId new_parent_id, const User& user);

    void remove(NodeId id, const User& user);
    void remove_recursive(NodeId id, const User& user);

    void save(std::ostream& output, const User& user) const;

    static VirtualFileSystem load(std::istream& input, std::unique_ptr<NodeDictionary> nodes, std::unique_ptr<FileIndexDictionary> index_dictionary);
};