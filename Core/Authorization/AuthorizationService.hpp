#pragma once
#include "Project/ProjectTypes.hpp"
class UserManager;
class VirtualFileSystem;
class AdminGrantStore;
class AuthorizationService {
  const UserManager& users_;
  const VirtualFileSystem& vfs_;
  const AdminGrantStore& grants_;
  NodeAccess direct_access(NodeId, UserId) const;

 public:
  AuthorizationService(const UserManager&, const VirtualFileSystem&,
                       const AdminGrantStore&);
  bool higher(UserId, UserId) const;
  bool covers(NodeId scope, NodeId node) const;
  bool grant_valid(const AdminGrant&, unsigned depth = 0) const;
  bool rule_valid(const AccessRule&, NodeId) const;
  bool matches(const AccessRule&, UserId) const;
  bool can(NodeId, UserId, AdminCapability) const;
  void require_source(NodeId, UserId, AdminGrantId, AdminCapability) const;
  bool can_edit(NodeId, const AccessRule&, UserId) const;
  NodeAccess access(NodeId, UserId) const;
  AccessExplanation explain(NodeId, UserId) const;
  ArraySequence<AccessRuleId> inherited_rules(NodeId,
                                              bool allow_only = false) const;
};
