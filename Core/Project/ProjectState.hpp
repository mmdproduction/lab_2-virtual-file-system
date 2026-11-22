#pragma once
#include "Authorization/AdminGrantStore.hpp"
#include "ProjectTypes.hpp"
#include "VirtualFileSystem/UserManager.hpp"
#include "VirtualFileSystem/VirtualFileSystem.hpp"
class ProjectState {
 public:
  UserManager users;
  VirtualFileSystem vfs;
  AdminGrantStore grants;
  AccessRuleId new_rule_id() const;
  std::uint64_t revision = 0;
  bool migrated = false;
  ProjectState();
  std::unique_ptr<ProjectState> clone() const;
  ChangeImpact revocation(AdminGrantId) const;
  void apply_revocation(const ChangeImpact&);
  std::pair<NodeId, AccessRule> rule(AccessRuleId) const;
};
