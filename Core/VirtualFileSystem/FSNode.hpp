#pragma once

#include <cstdint>
#include <string>

#include "Sequence/ArraySequence/ArraySequence.hpp"
#include "AccessPolicy.hpp"

using NodeId = std::uint64_t;

inline constexpr NodeId kInvalidNodeId = 0;

enum class FSType {
    File,
    Folder
};

enum class FileIcon : std::uint8_t {
    Default, Text, Image, Audio, Video, Code
};

struct FSNode {
    NodeId id = kInvalidNodeId;
    FSType type = FSType::Folder;
    std::string name;
    NodeId parent_id = kInvalidNodeId;
    std::string physical_path;
    ArraySequence<NodeId> children;
    AccessPolicy access_policy;
    FileIcon file_icon = FileIcon::Default;
};