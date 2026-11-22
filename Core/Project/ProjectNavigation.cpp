#include "Authorization/AuthorizationService.hpp"
#include "ProjectService.hpp"
#include "ProjectState.hpp"
std::string ProjectService::virtual_path(NodeId id, UserId a) const {
  node(id, a);
  std::string out;
  while (id && id != root_id()) {
    const auto& n = state_->vfs.get_node_unchecked(id);
    out = "/" + n.name + out;
    id = n.parent_id;
  }
  return out.empty() ? "/" : out;
}
std::string ProjectService::physical_path(NodeId id, UserId a) const {
  if (!access(id, a).read) throw AccessDenied("Read required to open a file");
  const auto& n = state_->vfs.get_node_unchecked(id);
  if (n.type != FSType::File) throw std::invalid_argument("Not a file");
  return n.physical_path;
}
ArraySequence<NodeId> ProjectService::find_files(const std::string& name,
                                                 UserId a) const {
  user(a);
  ArraySequence<NodeId> out;
  auto ids = state_->vfs.file_index_.find(name);
  for (std::size_t i = 0; i < ids.size(); ++i)
    if (access(ids.get(i), a).read) out.append(ids.get(i));
  return out;
}
ArraySequence<NodeInfo> ProjectService::administrative_children(
    NodeId id, UserId a) const {
  user(a);
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  auto manageable = [&](NodeId n) {
    return auth.can(n, a, AdminCapability::ManageAccess) ||
           auth.can(n, a, AdminCapability::DelegateAdministration);
  };
  auto needed = [&](NodeId n) {
    if (manageable(n)) return true;
    auto gs = admin_grants(a);
    for (std::size_t i = 0; i < gs.size(); ++i)
      if (gs.get(i).subject_id == a &&
          (HasCapability(gs.get(i).capabilities,
                         AdminCapability::ManageAccess) ||
           HasCapability(gs.get(i).capabilities,
                         AdminCapability::DelegateAdministration)) &&
          auth.covers(n, gs.get(i).scope_node_id))
        return true;
    return false;
  };
  if (!needed(id)) throw AccessDenied("No administrative scope here");
  const auto& n = state_->vfs.get_node_unchecked(id);
  ArraySequence<NodeInfo> out;
  for (std::size_t i = 0; i < n.children.size(); ++i) {
    const auto& c = state_->vfs.get_node_unchecked(n.children.get(i));
    if (needed(c.id))
      out.append(
          {c.id, c.parent_id, c.type, c.name, c.file_icon, !manageable(c.id)});
  }
  return out;
}
ArraySequence<AdminGrant> ProjectService::admin_sources(
    NodeId id, UserId a, AdminCapability cap) const {
  user(a);
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  ArraySequence<AdminGrant> out;
  auto ids = state_->grants.ids();
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const auto& g = state_->grants.get(ids.get(i));
    if (g.subject_id == a && HasCapability(g.capabilities, cap) &&
        auth.grant_valid(g) && auth.covers(g.scope_node_id, id))
      out.append(g);
  }
  return out;
}
std::string ProjectService::administrative_path(NodeId id, UserId a) const {
  user(a);
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  bool permitted = auth.can(id, a, AdminCapability::ManageAccess) ||
                   auth.can(id, a, AdminCapability::DelegateAdministration);
  auto gs = admin_grants(a);
  for (std::size_t i = 0; i < gs.size(); ++i)
    if (gs.get(i).subject_id == a &&
        (HasCapability(gs.get(i).capabilities, AdminCapability::ManageAccess) ||
         HasCapability(gs.get(i).capabilities,
                       AdminCapability::DelegateAdministration)) &&
        auth.covers(id, gs.get(i).scope_node_id))
      permitted = true;
  if (!permitted) throw AccessDenied("No administrative path");
  std::string out;
  while (id && id != root_id()) {
    const auto& n = state_->vfs.get_node_unchecked(id);
    out = "/" + n.name + out;
    id = n.parent_id;
  }
  return out.empty() ? "/" : out;
}
