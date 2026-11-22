#pragma once
#include <stdexcept>

#include "VirtualFileSystem/AccessTypes.hpp"
#include "VirtualFileSystem/FSNode.hpp"
class AccessDenied : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};
struct NodeAccess {
  bool read = false, write = false, navigate = false;
};
struct NodeInfo {
  NodeId id = 0, parent_id = 0;
  FSType type = FSType::Folder;
  std::string name;
  FileIcon file_icon = FileIcon::Default;
  bool navigation_only = false;
};
struct GrantRequest {
  UserId subject_id = 0;
  NodeId scope_node_id = 0;
  std::uint8_t capabilities = 0;
  AdminGrantId source_grant_id = 0;
};
struct RuleRequest {
  Principal principal;
  PermissionDecision read = PermissionDecision::Unset,
                     write = PermissionDecision::Unset;
};
struct ChangeImpact {
  std::uint64_t revision = 0;
  ArraySequence<AdminGrantId> grants;
  ArraySequence<AccessRuleId> rules;
};
struct RuleView {
  AccessRule rule;
  NodeId origin = 0;
  bool inherited = false, editable = false;
  std::string reason;
  bool allow_active = true;
};
struct PolicyView {
  bool inherits = false;
  ArraySequence<RuleView> rules;
};
struct AccessExplanation {
  NodeAccess access;
  ArraySequence<AccessRuleId> rules;
  ArraySequence<AdminGrantId> grants;
  std::string reason;
};
