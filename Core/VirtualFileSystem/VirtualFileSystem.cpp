#include "VirtualFileSystem.hpp"
#include "VirtualFileSystem/AccessTypes.hpp"

#include <filesystem>
#include <sstream>
#include <ostream>
#include <locale>
#include <limits>
#include <iomanip>
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
    root.access_policy.inherit_from_parent = false;

    nodes_->add(root.id, root);

    root_id_ = root.id;
    ++next_id_;
}

NodeId VirtualFileSystem::root_id() const noexcept {
    return root_id_;
}

const FSNode& VirtualFileSystem::get_node_unchecked(NodeId id) const {
    const NodeDictionary& dictionary = *nodes_;
    return dictionary.get(id);
}

const FSNode& VirtualFileSystem::get_node(
    NodeId id, const User& user) const {
    require_read(id, user);
    return get_node_unchecked(id);
}

NodeId VirtualFileSystem::create_directory(NodeId parent_id, const std::string& name, const User& user) {
    require_write(parent_id, user);

    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != std::string::npos) {
        throw std::invalid_argument("invalid directory name");
    }

    const FSNode& parent = get_node_unchecked(parent_id);

    if (parent.type != FSType::Folder) {
        throw std::invalid_argument("parent is not a folder");
    }

    for (std::size_t i = 0; i < parent.children.size(); ++i) {
        const FSNode& child =
            get_node_unchecked(parent.children.get(i));

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
    const std::string& physical_path,  const User& user) {
    require_write(parent_id, user); 
    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != std::string::npos) {
        throw std::invalid_argument("invalid file name");
    }

    const FSNode& parent = get_node_unchecked(parent_id);

    if (parent.type != FSType::Folder) {
        throw std::invalid_argument("parent is not a folder");
    }

    for (std::size_t i = 0; i < parent.children.size(); ++i) {
        const FSNode& child =
            get_node_unchecked(parent.children.get(i));

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
    const std::string& name, const User& user) const {
    if (user.id == kInvalidUserId) {
        throw std::invalid_argument("invalid user ID");
    }

    const ArraySequence<NodeId> candidates = file_index_.find(name);
    ArraySequence<NodeId> visible;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const NodeId id = candidates.get(i);
        if (get_access_rights(id, user).read) {
            visible.append(id);
        }
    }
    return visible;
}

void VirtualFileSystem::rename(
    NodeId id, const std::string& new_name, const User& user) {
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
    require_write(id, user);
    require_write(node.parent_id, user);

    if (node.name == new_name) {
        return;
    }

    const FSNode& parent = get_node_unchecked(node.parent_id);

    for (std::size_t i = 0; i < parent.children.size(); ++i) {
        const NodeId sibling_id = parent.children.get(i);

        if (sibling_id != id &&
            get_node_unchecked(sibling_id).name == new_name) {
            throw std::invalid_argument("name already exists");
        }
    }

    std::string prepared_name(new_name);

    if (node.type == FSType::File) {
        file_index_.rename(node.name, prepared_name, id);
    }

    node.name.swap(prepared_name);
}

void VirtualFileSystem::move(NodeId id, NodeId new_parent_id, const User& user) {
    if (id == root_id_) {
        throw std::invalid_argument(
            "cannot move the root directory");
    }

    const FSNode& node = get_node_unchecked(id);
    const FSNode& new_parent = get_node_unchecked(new_parent_id);

    require_write(id, user);
    require_write(node.parent_id, user);
    require_write(new_parent_id, user);

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

        ancestor_id = get_node_unchecked(ancestor_id).parent_id;
    }

    for (std::size_t i = 0; i < new_parent.children.size(); ++i) {
        const FSNode& child =
            get_node_unchecked(new_parent.children.get(i));

        if (child.name == node.name) {
            throw std::invalid_argument("name already exists");
        }
    }

    const NodeId old_parent_id = node.parent_id;
    const FSNode& old_parent = get_node_unchecked(old_parent_id);

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

void VirtualFileSystem::remove(NodeId id, const User& user) {
    if (id == root_id_) {
        throw std::invalid_argument(
            "cannot remove the root directory");
    }

    const FSNode& node = get_node_unchecked(id);

    require_write(id, user);
    require_write(node.parent_id, user);

    if (node.type == FSType::Folder &&
        node.children.size() != 0) {
        throw std::invalid_argument("directory is not empty");
    }

    const NodeId parent_id = node.parent_id;
    const FSNode& parent = get_node_unchecked(parent_id);

    std::size_t position = 0;

    while (position < parent.children.size() &&
           parent.children.get(position) != id) {
        ++position;
    }

    if (position == parent.children.size()) {
        throw std::logic_error(
            "node is missing from its parent's children");
    }

    ArraySequence<NodeId> updated_children(parent.children);
    updated_children.remove_at(position);

    FSNode& mutable_parent = nodes_->get(parent_id);

    if (node.type == FSType::File) {
        file_index_.remove(node.name, id);
    }

    nodes_->remove(id);
    mutable_parent.children.swap(updated_children);
}

void VirtualFileSystem::remove_recursive(NodeId id, const User& user) {
    if (id == root_id_) {
        throw std::invalid_argument(
            "cannot remove the root directory");
    }

    ArraySequence<NodeId> node_ids;
    node_ids.append(id);


    for (std::size_t i = 0; i < node_ids.size(); ++i) {
        const FSNode& node = get_node_unchecked(node_ids.get(i));

        require_write(node.id, user);
        require_write(node.parent_id, user);

        for (std::size_t j = 0; j < node.children.size(); ++j) {
            node_ids.append(node.children.get(j));
        }
    }
    for (std::size_t i = node_ids.size(); i > 0; --i) {
        remove(node_ids.get(i - 1), user);
    }
}

void VirtualFileSystem::save(std::ostream& output, const User& user) const {
    if (user.id == kInvalidUserId) {
    throw std::invalid_argument("invalid user ID");
    }

    if (!user.is_admin) {
        throw std::runtime_error(
            "access denied: only administrators can save the VFS");
    }
    if (!output) {
        throw std::runtime_error("output stream is not writable");
    }

    ArraySequence<NodeId> node_ids;
    node_ids.append(root_id_);

    for (std::size_t i = 0; i < node_ids.size(); ++i) {
        const FSNode& node = get_node_unchecked(node_ids.get(i));

        for (std::size_t j = 0; j < node.children.size(); ++j) {
            node_ids.append(node.children.get(j));
        }
    }

    std::ostringstream buffer;
    buffer.exceptions(std::ios::failbit | std::ios::badbit);
    buffer.imbue(std::locale::classic());

    bool custom_icons = false;
    for (std::size_t i = 0; i < node_ids.size(); ++i) {
        if (get_node_unchecked(node_ids.get(i)).file_icon != FileIcon::Default) {
            custom_icons = true;
            break;
        }
    }
    buffer << "VFS " << (custom_icons ? 3 : 2) << '\n' 
           << "ROOT " << root_id_ << '\n'
           << "NEXT_ID " << next_id_ << '\n'
           << "NODES " << node_ids.size() << '\n';

    for (std::size_t i = 0; i < node_ids.size(); ++i) {
        const FSNode& node = get_node_unchecked(node_ids.get(i));
        const char type = node.type == FSType::File ? 'F' : 'D';

        buffer << node.id << ' '
               << type << ' '
               << node.parent_id << ' '
               << std::quoted(node.name) << ' '
               << std::quoted(node.physical_path) << '\n';

        const AccessPolicy& policy = node.access_policy;
        buffer << "POLICY " << (policy.inherit_from_parent ? 1 : 0)
               << ' ' << policy.rules.size() << '\n';

        for (std::size_t j = 0; j < policy.rules.size(); ++j) {
            const GroupAccessRule& rule = policy.rules.get(j);
            buffer << "RULE " << rule.group_id << ' '
                   << (rule.rights.read ? 1 : 0) << ' '
                   << (rule.rights.write ? 1 : 0) << '\n';
        }
        if (custom_icons) {
            buffer << "ICON " << static_cast<unsigned>(node.file_icon) << '\n';
        }
    }

    const std::string text = buffer.str();

    if (text.size() > static_cast<std::size_t>(
            std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error(
            "saved data exceeds stream size limit");
    }

    output.write(
        text.data(),
        static_cast<std::streamsize>(text.size()));

    if (!output) {
        throw std::runtime_error(
            "failed to save virtual file system");
    }
}

AccessRights VirtualFileSystem::get_access_rights(
    NodeId id, const User& user) const {
    if (user.id == kInvalidUserId) {
        throw std::invalid_argument("invalid user ID");
    }

    const FSNode* current = &get_node_unchecked(id);

    if (user.is_admin) {
        return AccessRights{true, true};
    }

    AccessRights rights;

    while (true) {
        const AccessPolicy& policy = current->access_policy;

        for (std::size_t i = 0; i < policy.rules.size(); ++i) {
            const GroupAccessRule& rule = policy.rules.get(i);

            if (rule.group_id == kInvalidGroupId) {
                continue;
            }

            for (std::size_t j = 0; j < user.group_ids.size(); ++j) {
                if (user.group_ids.get(j) == rule.group_id) {
                    rights.read = rights.read || rule.rights.read;
                    rights.write = rights.write || rule.rights.write;
                    break;
                }
            }
        }

        if ((rights.read && rights.write) ||
            !policy.inherit_from_parent ||
            current->parent_id == kInvalidNodeId) {
            return rights;
        }

        current = &get_node_unchecked(current->parent_id);
    }
}


void VirtualFileSystem::set_access_policy(
    NodeId id, const AccessPolicy& policy, const User& user) {
    if (user.id == kInvalidUserId) {
        throw std::invalid_argument("invalid user ID");
    }
    if (!user.is_admin) {
        throw std::runtime_error(
            "access denied: only administrators can change policies");
    }

    FSNode& node = nodes_->get(id);

    if (id == root_id_ && policy.inherit_from_parent) {
        throw std::invalid_argument("root policy cannot inherit");
    }

    for (std::size_t i = 0; i < policy.rules.size(); ++i) {
        const GroupId group_id = policy.rules.get(i).group_id;
        if (group_id == kInvalidGroupId) {
            throw std::invalid_argument("invalid group ID in access policy");
        }

        for (std::size_t j = 0; j < i; ++j) {
            if (policy.rules.get(j).group_id == group_id) {
                throw std::invalid_argument("duplicate group ID in access policy");
            }
        }
    }

    AccessPolicy prepared(policy);

    node.access_policy.rules.swap(prepared.rules);
    node.access_policy.inherit_from_parent = prepared.inherit_from_parent;
}

void VirtualFileSystem::require_write(NodeId id, const User& user) const {
    if (!get_access_rights(id, user).write) {
        throw std::runtime_error("access denied: write permission required");
    }
}

void VirtualFileSystem::require_read(NodeId id, const User& user) const {
    if (!get_access_rights(id, user).read) {
        throw std::runtime_error("access denied: read permission required");
    }
}

void VirtualFileSystem::set_file_icon(NodeId id, FileIcon icon, const User& user) {
    require_write(id, user);
    FSNode& node = nodes_->get(id);
    if (node.type != FSType::File) {
        throw std::invalid_argument("file icon can only be set on a file");
    }
    if (static_cast<unsigned>(icon) > static_cast<unsigned>(FileIcon::Code)) {
        throw std::invalid_argument("unknown file icon");
    }
    node.file_icon = icon;
}
