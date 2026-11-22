#include "ProjectService.hpp"

#include "Authorization/AuthorizationService.hpp"
#include "ProjectState.hpp"
ProjectService::ProjectService() : state_(std::make_unique<ProjectState>()) {}
ProjectService::~ProjectService() = default;
UserId ProjectService::superadmin_id() const noexcept {
  return state_->users.admin_id();
}
NodeId ProjectService::root_id() const noexcept {
  return state_->vfs.root_id();
}
std::uint64_t ProjectService::revision() const noexcept {
  return state_->revision;
}
bool ProjectService::migrated() const noexcept { return state_->migrated; }
User ProjectService::user(UserId id) const {
  if (!id) throw std::invalid_argument("Invalid user ID");
  return state_->users.get_user(id);
}
Group ProjectService::group(GroupId id) const {
  return state_->users.get_group(id);
}
ArraySequence<UserId> ProjectService::user_ids() const {
  return state_->users.user_ids();
}
ArraySequence<GroupId> ProjectService::group_ids() const {
  return state_->users.group_ids();
}
void ProjectService::require_superadmin(UserId a) const {
  if (user(a).role != UserRole::SuperAdmin)
    throw AccessDenied("Superadministrator required");
}
NodeAccess ProjectService::access(NodeId id, UserId a) const {
  user(a);
  return AuthorizationService(state_->users, state_->vfs, state_->grants)
      .access(id, a);
}
NodeInfo ProjectService::node(NodeId id, UserId a) const {
  auto rights = access(id, a);
  if (!rights.read && !rights.navigate) throw AccessDenied("Read required");
  const auto& n = state_->vfs.get_node_unchecked(id);
  return {n.id, n.parent_id, n.type, n.name, n.file_icon, !rights.read};
}
ArraySequence<NodeInfo> ProjectService::children(NodeId id, UserId a) const {
  node(id, a);
  ArraySequence<NodeInfo> out;
  const auto& n = state_->vfs.get_node_unchecked(id);
  for (std::size_t i = 0; i < n.children.size(); ++i) {
    try {
      out.append(node(n.children.get(i), a));
    } catch (const AccessDenied&) {
    }
  }
  return out;
}
void ProjectService::require_write(NodeId id, UserId a) const {
  if (!access(id, a).write) throw AccessDenied("Write required");
}
NodeId ProjectService::create_directory(NodeId id, const std::string& n,
                                        UserId a) {
  require_write(id, a);
  auto r = state_->vfs.create_directory(id, n);
  ++state_->revision;
  return r;
}
NodeId ProjectService::attach_file(NodeId id, const std::string& n,
                                   const std::string& p, UserId a) {
  require_write(id, a);
  auto r = state_->vfs.attach_file(id, n, p);
  ++state_->revision;
  return r;
}
void ProjectService::rename(NodeId id, const std::string& n, UserId a) {
  user(a);
  if (id == root_id()) throw std::invalid_argument("Cannot rename root");
  const auto& old = state_->vfs.get_node_unchecked(id);
  require_write(id, a);
  require_write(old.parent_id, a);
  bool change = old.name != n;
  state_->vfs.rename(id, n);
  if (change) ++state_->revision;
}
void ProjectService::set_file_icon(NodeId id, FileIcon v, UserId a) {
  require_write(id, a);
  bool change = state_->vfs.get_node_unchecked(id).file_icon != v;
  state_->vfs.set_file_icon(id, v);
  if (change) ++state_->revision;
}
UserId ProjectService::create_user(const std::string& n, UserId a) {
  require_superadmin(a);
  auto id = state_->users.create_user(n);
  ++state_->revision;
  return id;
}
GroupId ProjectService::create_group(const std::string& n, UserId a) {
  require_superadmin(a);
  auto id = state_->users.create_group(n);
  ++state_->revision;
  return id;
}
void ProjectService::rename_user(UserId id, const std::string& n, UserId a) {
  require_superadmin(a);
  bool change = user(id).name != n;
  state_->users.rename_user(id, n);
  if (change) ++state_->revision;
}
void ProjectService::rename_group(GroupId id, const std::string& n, UserId a) {
  require_superadmin(a);
  bool change = group(id).name != n;
  state_->users.rename_group(id, n);
  if (change) ++state_->revision;
}
void ProjectService::set_membership(UserId u, GroupId g, bool value, UserId a) {
  require_superadmin(a);
  const auto& membership = state_->users.get_user(u).group_ids;
  const auto before = membership.size();
  if (value)
    state_->users.add_user_to_group(u, g);
  else
    state_->users.remove_user_from_group(u, g);
  if (before != membership.size()) ++state_->revision;
}
