#include "AuthorizationService.hpp"

#include "AdminGrantStore.hpp"
#include "VirtualFileSystem/UserManager.hpp"
#include "VirtualFileSystem/VirtualFileSystem.hpp"
AuthorizationService::AuthorizationService(const UserManager& u,
                                           const VirtualFileSystem& v,
                                           const AdminGrantStore& g)
    : users_(u), vfs_(v), grants_(g) {}
bool AuthorizationService::higher(UserId a, UserId b) const {
  const auto& x = users_.get_user(a);
  const auto& y = users_.get_user(b);
  if (a == b || y.role == UserRole::SuperAdmin) return false;
  if (x.role == UserRole::SuperAdmin) return true;
  return x.role == UserRole::Admin &&
         (y.role == UserRole::User || x.admin_level > y.admin_level);
}
bool AuthorizationService::covers(NodeId scope, NodeId node) const {
  std::size_t depth = 0;
  while (node && depth++ <= vfs_.nodes_->size()) {
    if (node == scope) return true;
    node = vfs_.get_node_unchecked(node).parent_id;
  }
  return false;
}
bool AuthorizationService::grant_valid(const AdminGrant& g,
                                       unsigned depth) const {
  if (depth > 255 || !g.id || !g.capabilities ||
      !IsValidCapabilities(g.capabilities))
    return false;
  try {
    const auto& subject = users_.get_user(g.subject_id);
    const auto& issuer = users_.get_user(g.issuer_id);
    if (subject.role != UserRole::Admin || !higher(g.issuer_id, g.subject_id) ||
        vfs_.get_node_unchecked(g.scope_node_id).type != FSType::Folder)
      return false;
    if (issuer.role == UserRole::SuperAdmin) return g.parent_grant_id.is_nil();
    if (!g.parent_grant_id) return false;
    const auto& parent = grants_.get(g.parent_grant_id);
    return parent.subject_id == g.issuer_id && grant_valid(parent, depth + 1) &&
           HasCapability(parent.capabilities,
                         AdminCapability::DelegateAdministration) &&
           (g.capabilities & parent.capabilities) == g.capabilities &&
           covers(parent.scope_node_id, g.scope_node_id);
  } catch (const std::out_of_range&) {
    return false;
  }
}
bool AuthorizationService::rule_valid(const AccessRule& r, NodeId node) const {
  try {
    const auto& author = users_.get_user(r.issuer_id);
    if (author.role == UserRole::SuperAdmin) return r.source_grant_id.is_nil();
    if (author.role != UserRole::Admin || !r.source_grant_id) return false;
    const auto& g = grants_.get(r.source_grant_id);
    return g.subject_id == r.issuer_id && grant_valid(g) &&
           HasCapability(g.capabilities, AdminCapability::ManageAccess) &&
           covers(g.scope_node_id, node);
  } catch (const std::out_of_range&) {
    return false;
  }
}
bool AuthorizationService::matches(const AccessRule& r, UserId actor) const {
  const auto& u = users_.get_user(actor);
  if (!higher(r.issuer_id, actor)) return false;
  if (r.principal.kind == PrincipalKind::User) return r.principal.id == actor;
  for (std::size_t i = 0; i < u.group_ids.size(); ++i)
    if (u.group_ids.get(i) == r.principal.id) return true;
  return false;
}
bool AuthorizationService::can(NodeId id, UserId actor,
                               AdminCapability c) const {
  vfs_.get_node_unchecked(id);
  const auto& u = users_.get_user(actor);
  if (u.role == UserRole::SuperAdmin) return true;
  if (u.role != UserRole::Admin) return false;
  auto ids = grants_.ids();
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const auto& g = grants_.get(ids.get(i));
    if (g.subject_id == actor && HasCapability(g.capabilities, c) &&
        grant_valid(g) && covers(g.scope_node_id, id))
      return true;
  }
  return false;
}
void AuthorizationService::require_source(NodeId node, UserId actor,
                                          AdminGrantId source,
                                          AdminCapability cap) const {
  vfs_.get_node_unchecked(node);
  const auto& u = users_.get_user(actor);
  if (u.role == UserRole::SuperAdmin) {
    if (source)
      throw std::invalid_argument("Superadministrator source must be zero");
    return;
  }
  if (u.role != UserRole::Admin || !source)
    throw AccessDenied("Administrative assignment required");
  const auto& g = grants_.get(source);
  if (g.subject_id != actor || !grant_valid(g) ||
      !HasCapability(g.capabilities, cap) || !covers(g.scope_node_id, node))
    throw AccessDenied("Assignment does not authorize this action or scope");
}
bool AuthorizationService::can_edit(NodeId origin, const AccessRule& r,
                                    UserId a) const {
  return can(origin, a, AdminCapability::ManageAccess) &&
         (a == r.issuer_id || higher(a, r.issuer_id));
}
NodeAccess AuthorizationService::direct_access(NodeId id, UserId actor) const {
  const auto& u = users_.get_user(actor);
  vfs_.get_node_unchecked(id);
  if (u.role == UserRole::SuperAdmin) return {true, true, true};
  bool allow_read = can(id, actor, AdminCapability::ReadContent),
       allow_write = can(id, actor, AdminCapability::WriteContent),
       deny_read = false, deny_write = false, inherit_allow = true;
  for (NodeId cur = id; cur; cur = vfs_.get_node_unchecked(cur).parent_id) {
    const auto& policy = vfs_.get_node_unchecked(cur).access_policy;
    for (std::size_t i = 0; i < policy.rules.size(); ++i) {
      const auto& r = policy.rules.get(i);
      if (!rule_valid(r, cur) || !matches(r, actor)) continue;
      deny_read |= r.read == PermissionDecision::Deny;
      deny_write |= r.write == PermissionDecision::Deny;
      if (inherit_allow) {
        allow_read |= r.read == PermissionDecision::Allow;
        allow_write |= r.write == PermissionDecision::Allow;
      }
    }
    inherit_allow = inherit_allow && policy.inherit_from_parent;
  }
  return {allow_read && !deny_read, allow_write && !deny_write, false};
}
NodeAccess AuthorizationService::access(NodeId id, UserId actor) const {
  auto rights = direct_access(id, actor);
  if (rights.read) {
    rights.navigate = true;
    return rights;
  }
  const auto& node = vfs_.get_node_unchecked(id);
  if (node.type != FSType::Folder) return rights;
  ArraySequence<NodeId> queue;
  queue.append(id);
  for (std::size_t i = 0; i < queue.size(); ++i) {
    const auto& n = vfs_.get_node_unchecked(queue.get(i));
    for (std::size_t j = 0; j < n.children.size(); ++j) {
      auto c = n.children.get(j);
      if (direct_access(c, actor).read) {
        rights.navigate = true;
        return rights;
      }
      queue.append(c);
    }
  }
  return rights;
}
AccessExplanation AuthorizationService::explain(NodeId id, UserId actor) const {
  AccessExplanation e;
  e.access = access(id, actor);
  bool inherited = true;
  bool denied = false;
  for (NodeId cur = id; cur; cur = vfs_.get_node_unchecked(cur).parent_id) {
    const auto& p = vfs_.get_node_unchecked(cur).access_policy;
    for (std::size_t i = 0; i < p.rules.size(); ++i) {
      const auto& r = p.rules.get(i);
      if (rule_valid(r, cur) && matches(r, actor) &&
          ((inherited && (r.read == PermissionDecision::Allow ||
                          r.write == PermissionDecision::Allow)) ||
           r.read == PermissionDecision::Deny ||
           r.write == PermissionDecision::Deny)) {
        e.rules.append(r.id);
        denied |= r.read == PermissionDecision::Deny ||
                  r.write == PermissionDecision::Deny;
      }
    }
    inherited = inherited && p.inherit_from_parent;
  }
  auto ids = grants_.ids();
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const auto& g = grants_.get(ids.get(i));
    if (g.subject_id == actor && grant_valid(g) && covers(g.scope_node_id, id))
      e.grants.append(g.id);
  }
  e.reason = denied ? "Explicit or inherited deny"
                    : (e.access.read || e.access.write
                           ? "Allowed by rule or assignment"
                           : "No applicable permission");
  return e;
}
ArraySequence<AccessRuleId> AuthorizationService::inherited_rules(
    NodeId id, bool allow_only) const {
  ArraySequence<AccessRuleId> out;
  bool inherit = vfs_.get_node_unchecked(id).access_policy.inherit_from_parent;
  for (NodeId cur = vfs_.get_node_unchecked(id).parent_id; cur;
       cur = vfs_.get_node_unchecked(cur).parent_id) {
    const auto& p = vfs_.get_node_unchecked(cur).access_policy;
    for (std::size_t i = 0; i < p.rules.size(); ++i) {
      const auto& r = p.rules.get(i);
      if ((inherit && (r.read == PermissionDecision::Allow ||
                       r.write == PermissionDecision::Allow)) ||
          (!allow_only && (r.read == PermissionDecision::Deny ||
                           r.write == PermissionDecision::Deny)))
        out.append(r.id);
    }
    inherit = inherit && p.inherit_from_parent;
  }
  return out;
}
