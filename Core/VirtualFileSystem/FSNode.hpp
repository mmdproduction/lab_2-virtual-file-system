#pragma once

#include <cstdint>
#include <string>

#include "Sequence/ArraySequence/ArraySequence.hpp"

using NodeId = std::uint64_t;

inline constexpr NodeId kInvalidNodeId = 0;

enum class FSType {
    File,
    Folder
};

struct FSNode {
    NodeId id = kInvalidNodeId;
    FSType type = FSType::Folder;
    std::string name;
    NodeId parent_id = kInvalidNodeId;
    std::string physical_path;
    ArraySequence<NodeId> children;
};