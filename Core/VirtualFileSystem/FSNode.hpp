#pragma once
#include <cstdint>
#include <string>

#include "AccessPolicy.hpp"
#include "Identifiers.hpp"
#include "Sequence/ArraySequence/ArraySequence.hpp"

enum class FSType { File, Folder };

enum class FileIcon : std::uint8_t { Default, Text, Image, Audio, Video, Code };

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
// GUID