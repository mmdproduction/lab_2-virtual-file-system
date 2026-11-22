#pragma once
#include <memory>

#include "ProjectTypes.hpp"
class ProjectState;
class ProjectCodec;
class ProjectStore;
class ProjectService {
  friend class ProjectCodec;
  friend class ProjectStore;
  std::unique_ptr<ProjectState> state_;
  void require_write(NodeId, UserId) const;

 public:
  ProjectService();
  ~ProjectService();
  std::string save(UserId) const;
  std::unique_ptr<ProjectService> load(const std::string&, UserId) const;
  UserId superadmin_id() const noexcept;
  NodeId root_id() const noexcept;
  std::uint64_t revision() const noexcept;
  bool migrated() const noexcept;
  User user(UserId) const;
  Group group(GroupId) const;
  ArraySequence<UserId> user_ids() const;
  ArraySequence<GroupId> group_ids() const;
  void require_superadmin(UserId) const;
  NodeAccess access(NodeId, UserId) const;
  NodeInfo node(NodeId, UserId) const;
  ArraySequence<NodeInfo> children(NodeId, UserId) const;
  ArraySequence<NodeId> find_files(const std::string&, UserId) const;
  std::string virtual_path(NodeId, UserId) const;
  std::string physical_path(NodeId, UserId) const;
  NodeId create_directory(NodeId, const std::string&, UserId);
  NodeId attach_file(NodeId, const std::string&, const std::string&, UserId);
  bool can_move(NodeId, NodeId, UserId) const;
  void rename(NodeId, const std::string&, UserId);
  void move(NodeId, NodeId, UserId);
  void remove(NodeId, bool, UserId);
  void set_file_icon(NodeId, FileIcon, UserId);
  UserId create_user(const std::string&, UserId);
  GroupId create_group(const std::string&, UserId);
  void rename_user(UserId, const std::string&, UserId);
  void rename_group(GroupId, const std::string&, UserId);
  void set_membership(UserId, GroupId, bool, UserId);
  AdminGrantId grant_admin(const GrantRequest&, UserId);
  ChangeImpact preview_revoke(AdminGrantId, UserId) const;
  void revoke(AdminGrantId, UserId, std::uint64_t);
  ChangeImpact preview_role_change(UserId, UserRole, unsigned, UserId) const;
  void set_role(UserId, UserRole, unsigned, UserId, std::uint64_t);
  AccessRuleId upsert_rule(NodeId, AccessRuleId, const RuleRequest&, UserId,
                           AdminGrantId);
  void remove_rule(NodeId, AccessRuleId, UserId, AdminGrantId);
  void set_inheritance(NodeId, bool, UserId, AdminGrantId);
  AccessExplanation explain(NodeId, UserId, UserId) const;
  PolicyView policy_view(NodeId, UserId) const;
  ArraySequence<AdminGrant> admin_grants(UserId) const;
  ArraySequence<NodeInfo> administrative_children(NodeId, UserId) const;
  bool can_administer(NodeId, UserId, AdminCapability) const;
  ArraySequence<AdminGrant> admin_sources(NodeId, UserId,
                                          AdminCapability) const;
  std::string administrative_path(NodeId, UserId) const;
};
