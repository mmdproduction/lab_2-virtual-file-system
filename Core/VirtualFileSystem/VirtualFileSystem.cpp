#include "VirtualFileSystem.hpp"

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