#pragma once
#include <memory>
#include <string>

#include "Dictionary/IDictionary.hpp"
#include "FSNode.hpp"
#include "FileNameIndex.hpp"
using NodeDictionary = IDictionary<NodeId, FSNode>;
class VirtualFileSystem {
  friend class ProjectStore;
  friend class ProjectService;
  friend class ProjectState;
  friend class AuthorizationService;
  friend class AuthorizationGraph;
  friend class ProjectCodec;
  std::unique_ptr<NodeDictionary> nodes_;
  FileNameIndex file_index_;
  NodeId root_id_ = 0;
  NodeId next_id_ = 1;
  const FSNode& get_node_unchecked(NodeId) const;
  NodeId create_directory(NodeId, const std::string&);
  NodeId attach_file(NodeId, const std::string&, const std::string&);
  void rename(NodeId, const std::string&);
  void move(NodeId, NodeId);
  void remove(NodeId);
  void remove_recursive(NodeId);
  void set_file_icon(NodeId, FileIcon);

 public:
  explicit VirtualFileSystem(std::unique_ptr<NodeDictionary>,
                             std::unique_ptr<FileIndexDictionary>);
  NodeId root_id() const noexcept;
};
