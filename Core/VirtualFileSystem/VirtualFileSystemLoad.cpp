#include "VirtualFileSystem.hpp"

#include <charconv>
#include <cstddef>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {

std::string ReadVfsToken(std::istream& input) {
    std::string token;
    if (!(input >> token)) {
        throw std::runtime_error("unexpected end of VFS data");
    }
    return token;
}

void ExpectVfsToken(std::istream& input, const char* expected) {
    if (ReadVfsToken(input) != expected) {
        throw std::runtime_error("invalid VFS header");
    }
}

NodeId ReadVfsNumber(std::istream& input) {
    const std::string token = ReadVfsToken(input);
    NodeId value = 0;
    const char* end = token.data() + token.size();
    const auto result = std::from_chars(token.data(), end, value);

    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error("invalid unsigned decimal number in VFS data");
    }
    return value;
}

std::string ReadVfsString(std::istream& input) {
    input >> std::ws;
    if (input.peek() != '"') {
        throw std::runtime_error("expected a quoted string in VFS data");
    }

    std::string value;
    if (!(input >> std::quoted(value))) {
        throw std::runtime_error("incomplete quoted string in VFS data");
    }
    return value;
}


bool ReadVfsBool(std::istream& input) {
    const NodeId value = ReadVfsNumber(input);
    if (value > 1) {
        throw std::runtime_error("VFS boolean flag must be 0 or 1");
    }
    return value == 1;
}

AccessPolicy ReadVfsPolicy(std::istream& input) {
    ExpectVfsToken(input, "POLICY");
    AccessPolicy policy;
    policy.inherit_from_parent = ReadVfsBool(input);
    const NodeId saved_count = ReadVfsNumber(input);
    if (saved_count > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("invalid access rule count");
    }

    const std::size_t count = static_cast<std::size_t>(saved_count);
    for (std::size_t i = 0; i < count; ++i) {
        ExpectVfsToken(input, "RULE");
        GroupAccessRule rule;
        rule.group_id = ReadVfsNumber(input);
        rule.rights.read = ReadVfsBool(input);
        rule.rights.write = ReadVfsBool(input);
        if (rule.group_id == kInvalidGroupId) {
            throw std::runtime_error("invalid group ID in saved access policy");
        }
        for (std::size_t j = 0; j < policy.rules.size(); ++j) {
            if (policy.rules.get(j).group_id == rule.group_id) {
                throw std::runtime_error("duplicate group ID in saved access policy");
            }
        }
        policy.rules.append(rule);
    }
    return policy;
}

}  

VirtualFileSystem VirtualFileSystem::load(
    std::istream& input,
    std::unique_ptr<NodeDictionary> nodes,
    std::unique_ptr<FileIndexDictionary> index_dictionary) {
    
    VirtualFileSystem result(std::move(nodes), std::move(index_dictionary));

    if (!input) {
        throw std::runtime_error("input stream is not readable");
    }

    
    std::istream reader(input.rdbuf());
    reader.exceptions(std::ios::badbit);
    reader.imbue(std::locale::classic());

    ExpectVfsToken(reader, "VFS");
    const NodeId version = ReadVfsNumber(reader);
    if (version != 1 && version != 2) {
        throw std::runtime_error("unsupported VFS format version");
    }
    ExpectVfsToken(reader, "ROOT");
    const NodeId saved_root = ReadVfsNumber(reader);
    ExpectVfsToken(reader, "NEXT_ID");
    const NodeId saved_next = ReadVfsNumber(reader);
    ExpectVfsToken(reader, "NODES");
    const NodeId saved_count = ReadVfsNumber(reader);

    if (saved_root == kInvalidNodeId || saved_count == 0 ||
        saved_count > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("invalid root ID or node count");
    }

    result.nodes_->clear();
    result.root_id_ = saved_root;
    result.next_id_ = saved_next;

    ArraySequence<NodeId> record_ids;
    NodeId max_id = kInvalidNodeId;
    const std::size_t count = static_cast<std::size_t>(saved_count);

    for (std::size_t i = 0; i < count; ++i) {
        FSNode node;
        node.id = ReadVfsNumber(reader);
        const std::string type = ReadVfsToken(reader);
        node.parent_id = ReadVfsNumber(reader);
        node.name = ReadVfsString(reader);
        node.physical_path = ReadVfsString(reader);

        if (type != "F" && type != "D") {
            throw std::runtime_error("unknown VFS node type");
        }
        node.type = type == "F" ? FSType::File : FSType::Folder;

        if (node.id == kInvalidNodeId || result.nodes_->contains_key(node.id)) {
            throw std::runtime_error("zero or duplicate VFS node ID");
        }
        if (node.id != saved_root &&
            (node.name.empty() || node.name == "." || node.name == ".." ||
             node.name.find('/') != std::string::npos)) {
            throw std::runtime_error("invalid VFS node name");
        }
        if ((node.type == FSType::Folder && !node.physical_path.empty()) ||
            (node.type == FSType::File && node.physical_path.empty())) {
            throw std::runtime_error("invalid physical path for VFS node type");
        }

        if (version == 2) {
            node.access_policy = ReadVfsPolicy(reader);
        } else {
            node.access_policy.inherit_from_parent = node.id != saved_root;
        }

        result.nodes_->add(node.id, node);
        record_ids.append(node.id);
        if (node.id > max_id) {
            max_id = node.id;
        }
    }

    reader >> std::ws;
    if (!reader.eof()) {
        throw std::runtime_error("unexpected data after VFS records");
    }
    if (!result.nodes_->contains_key(saved_root)) {
        throw std::runtime_error("VFS root node is missing");
    }

    const FSNode& root = result.get_node_unchecked(saved_root);
    if (root.type != FSType::Folder || root.parent_id != kInvalidNodeId ||
        root.name != "/" || root.access_policy.inherit_from_parent) {
        throw std::runtime_error("invalid VFS root node");
    }

    if (saved_next != kInvalidNodeId && saved_next <= max_id) {
        throw std::runtime_error("next VFS node ID would reuse an existing ID");
    }

    for (std::size_t i = 0; i < record_ids.size(); ++i) {
        const NodeId id = record_ids.get(i);
        if (id == saved_root) {
            continue;
        }
        const FSNode& node = result.get_node_unchecked(id);
        if (node.parent_id == kInvalidNodeId || node.parent_id == id ||
            !result.nodes_->contains_key(node.parent_id)) {
            throw std::runtime_error("missing or invalid VFS parent");
        }

        FSNode& parent = result.nodes_->get(node.parent_id);
        if (parent.type != FSType::Folder) {
            throw std::runtime_error("a file cannot be a VFS parent");
        }
        for (std::size_t j = 0; j < parent.children.size(); ++j) {
            if (result.get_node_unchecked(parent.children.get(j)).name == node.name) {
                throw std::runtime_error("duplicate names in a VFS directory");
            }
        }
        parent.children.append(id);
    }

    ArraySequence<NodeId> reachable_ids;
    reachable_ids.append(saved_root);
    for (std::size_t i = 0; i < reachable_ids.size(); ++i) {
        const FSNode& node = result.get_node_unchecked(reachable_ids.get(i));
        for (std::size_t j = 0; j < node.children.size(); ++j) {
            reachable_ids.append(node.children.get(j));
        }
    }
    if (reachable_ids.size() != record_ids.size()) {
        throw std::runtime_error("VFS contains a cycle or unreachable nodes");
    }

    for (std::size_t i = 0; i < record_ids.size(); ++i) {
        const FSNode& node = result.get_node_unchecked(record_ids.get(i));
        if (node.type == FSType::File) {
            result.file_index_.add(node.name, node.id);
        }
    }

    return result;
}
