#include "VirtualFileSystem.hpp"

#include <filesystem>
#include <stdexcept>
#include <utility>

VirtualFileSystem::VirtualFileSystem(
    std::unique_ptr<NodeDictionary> nodes)
    : nodes_(std::move(nodes)) {
    if (!nodes_) {
        throw std::invalid_argument(
            "node dictionary must not be null");
    }

    if (nodes_->size() != 0) {
        throw std::invalid_argument(
            "node dictionary must be empty");
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
        nodes_->get(parent_id).children = updated_children;
    } catch (...) {
        nodes_->remove(file.id);
        throw;
    }

    ++next_id_;
    return file.id;
}