#include "VirtualFileSystem.hpp"

#include <filesystem>
#include <stdexcept>
#include <utility>

VirtualFileSystem::VirtualFileSystem(
    std::unique_ptr<NodeDictionary> nodes,
    std::unique_ptr<FileIndexDictionary> index_dictionary)
    : nodes_(std::move(nodes)),
      file_index_(std::move(index_dictionary)) {
    if (!nodes_) {
        throw std::invalid_argument(
            "Node dictionary must not be null");
    }

    if (nodes_->size() != 0) {
        throw std::invalid_argument(
            "Node dictionary must be empty");
    }

    FSNode root;
    root.id = next_id_;
    root.type = FSType::Folder;
    root.name = "/";
    root.parent_id = kInvalidNodeId;

    nodes_->add(root.id, root);

    root_id_ = root.id;
    ++next_id_;
}

NodeId VirtualFileSystem::root_id() const noexcept {
    return root_id_;
}

const FSNode& VirtualFileSystem::get_node(NodeId id) const {
    const NodeDictionary& dictionary = *nodes_;
    return dictionary.get(id);
}

NodeId VirtualFileSystem::create_directory(
    NodeId parent_id, const std::string& name) {
    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != std::string::npos) {
        throw std::invalid_argument("invalid directory name");
    }

    const FSNode& parent = get_node(parent_id);

    if (parent.type != FSType::Folder) {
        throw std::invalid_argument("parent is not a folder");
    }

    for (std::size_t i = 0; i < parent.children.size(); ++i) {
        const FSNode& child =
            get_node(parent.children.get(i));

        if (child.name == name) {
            throw std::invalid_argument("name already exists");
        }
    }

    if (next_id_ == kInvalidNodeId) {
        throw std::overflow_error("node ID space exhausted");
    }

    FSNode directory;
    directory.id = next_id_;
    directory.type = FSType::Folder;
    directory.name = name;
    directory.parent_id = parent_id;

    ArraySequence<NodeId> updated_children(parent.children);
    updated_children.append(directory.id);

    nodes_->add(directory.id, directory);

    try {
        nodes_->get(parent_id).children = updated_children;
    } catch (...) {
        nodes_->remove(directory.id);
        throw;
    }

    ++next_id_;
    return directory.id;
}


NodeId VirtualFileSystem::attach_file(
    NodeId parent_id,
    const std::string& name,
    const std::string& physical_path) {
    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != std::string::npos) {
        throw std::invalid_argument("invalid file name");
    }

    const FSNode& parent = get_node(parent_id);

    if (parent.type != FSType::Folder) {
        throw std::invalid_argument("parent is not a folder");
    }

    for (std::size_t i = 0; i < parent.children.size(); ++i) {
        const FSNode& child =
            get_node(parent.children.get(i));

        if (child.name == name) {
            throw std::invalid_argument("name already exists");
        }
    }

    if (physical_path.empty() ||
        !std::filesystem::is_regular_file(physical_path)) {
        throw std::invalid_argument(
            "physical path must point to an existing regular file");
    }

    if (next_id_ == kInvalidNodeId) {
        throw std::overflow_error("node ID space exhausted");
    }

    FSNode file;
    file.id = next_id_;
    file.type = FSType::File;
    file.name = name;
    file.parent_id = parent_id;
    file.physical_path =
        std::filesystem::canonical(physical_path).string();

    ArraySequence<NodeId> updated_children(parent.children);
    updated_children.append(file.id);

    nodes_->add(file.id, file);

    try {
        FSNode& mutable_parent = nodes_->get(parent_id);

        file_index_.add(file.name, file.id);

        mutable_parent.children.swap(updated_children);
    } catch (...) {
        nodes_->remove(file.id);
        throw;
    }

    ++next_id_;
    return file.id;
}

ArraySequence<NodeId> VirtualFileSystem::find_files(
    const std::string& name) const {
    return file_index_.find(name);
}

void VirtualFileSystem::rename(
    NodeId id, const std::string& new_name) {
    if (id == root_id_) {
        throw std::invalid_argument(
            "cannot rename the root directory");
    }

    if (new_name.empty() || new_name == "." ||
        new_name == ".." ||
        new_name.find('/') != std::string::npos) {
        throw std::invalid_argument("invalid node name");
    }

    FSNode& node = nodes_->get(id);

    if (node.name == new_name) {
        return;
    }

    const FSNode& parent = get_node(node.parent_id);

    for (std::size_t i = 0; i < parent.children.size(); ++i) {
        const NodeId sibling_id = parent.children.get(i);

        if (sibling_id != id &&
            get_node(sibling_id).name == new_name) {
            throw std::invalid_argument("name already exists");
        }
    }

    std::string prepared_name(new_name);

    if (node.type == FSType::File) {
        file_index_.rename(node.name, prepared_name, id);
    }

    node.name.swap(prepared_name);
}

void VirtualFileSystem::move(NodeId id, NodeId new_parent_id) {
    if (id == root_id_) {
        throw std::invalid_argument(
            "cannot move the root directory");
    }

    const FSNode& node = get_node(id);
    const FSNode& new_parent = get_node(new_parent_id);

    if (new_parent.type != FSType::Folder) {
        throw std::invalid_argument("new parent is not a folder");
    }

    if (node.parent_id == new_parent_id) {
        return;
    }

    NodeId ancestor_id = new_parent_id;

    while (ancestor_id != kInvalidNodeId) {
        if (ancestor_id == id) {
            throw std::invalid_argument(
                "cannot move a directory into itself or its descendant");
        }

        ancestor_id = get_node(ancestor_id).parent_id;
    }

    for (std::size_t i = 0; i < new_parent.children.size(); ++i) {
        const FSNode& child =
            get_node(new_parent.children.get(i));

        if (child.name == node.name) {
            throw std::invalid_argument("name already exists");
        }
    }

    const NodeId old_parent_id = node.parent_id;
    const FSNode& old_parent = get_node(old_parent_id);

    std::size_t position = 0;

    while (position < old_parent.children.size() &&
           old_parent.children.get(position) != id) {
        ++position;
    }

    if (position == old_parent.children.size()) {
        throw std::logic_error(
            "node is missing from its parent's children");
    }

    ArraySequence<NodeId> updated_old_children(
        old_parent.children);
    updated_old_children.remove_at(position);

    ArraySequence<NodeId> updated_new_children(
        new_parent.children);
    updated_new_children.append(id);

    FSNode& mutable_old_parent = nodes_->get(old_parent_id);
    FSNode& mutable_new_parent = nodes_->get(new_parent_id);
    FSNode& mutable_node = nodes_->get(id);

    mutable_old_parent.children.swap(updated_old_children);
    mutable_new_parent.children.swap(updated_new_children);
    mutable_node.parent_id = new_parent_id;
}