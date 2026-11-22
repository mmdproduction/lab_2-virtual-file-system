#include "Authorization/AuthorizationGraph.hpp"
#include "Authorization/AuthorizationService.hpp"
#include "ProjectHelpers.hpp"
#include "ProjectService.hpp"
#include "ProjectState.hpp"
namespace {
void RoleInput(UserRole role, unsigned level) {
  if (!((role == UserRole::User && level == 0) ||
        (role == UserRole::Admin && IsValidAdminLevel(level))))
    throw std::invalid_argument(
        "Choose User with level 0 or Admin with level 1..255");
}
}  // namespace
bool ProjectService::can_administer(NodeId id, UserId a,
                                    AdminCapability cap) const {
  user(a);
  return AuthorizationService(state_->users, state_->vfs, state_->grants)
      .can(id, a, cap);
}
AdminGrantId ProjectService::grant_admin(const GrantRequest& r, UserId a) {
  auto u = user(a);
  auto target = user(r.subject_id);
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  if (!r.capabilities || !IsValidCapabilities(r.capabilities))
    throw std::invalid_argument("Invalid capability mask");
  if (target.role != UserRole::Admin || !auth.higher(a, r.subject_id))
    throw AccessDenied("Recipient must be a lower administrator");
  if (state_->vfs.get_node_unchecked(r.scope_node_id).type != FSType::Folder)
    throw std::invalid_argument("Scope must be a folder");
  auth.require_source(r.scope_node_id, a, r.source_grant_id,
                      AdminCapability::DelegateAdministration);
  if (u.role != UserRole::SuperAdmin) {
    const auto& parent = state_->grants.get(r.source_grant_id);
    if ((parent.capabilities & r.capabilities) != r.capabilities)
      throw AccessDenied(
          "Cannot delegate a union or a larger set of capabilities");
  }
  auto id = GenerateUniqueGuid([&](Guid candidate) {
    return state_->grants.grants_->contains_key(candidate);
  });
  AdminGrant g{
      id, r.subject_id, r.scope_node_id, r.capabilities, a,
      r.source_grant_id};
  state_->grants.grants_->add(g.id, g);
  ++state_->revision;
  return g.id;
}
ChangeImpact ProjectService::preview_revoke(AdminGrantId id, UserId a) const {
  auto u = user(a);
  const auto& g = state_->grants.get(id);
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  if (u.role != UserRole::SuperAdmin &&
      !(auth.can(g.scope_node_id, a, AdminCapability::DelegateAdministration) &&
        auth.higher(a, g.subject_id) &&
        (a == g.issuer_id || auth.higher(a, g.issuer_id))))
    throw AccessDenied("Cannot revoke this assignment");
  return state_->revocation(id);
}
void ProjectService::revoke(AdminGrantId id, UserId a, std::uint64_t expected) {
  auto impact = preview_revoke(id, a);
  if (expected != revision())
    throw std::runtime_error("Project changed: refresh the preview");
  auto next = state_->clone();
  next->apply_revocation(impact);
  AuthorizationGraph::validate(*next);
  ++next->revision;
  state_.swap(next);
}
ChangeImpact ProjectService::preview_role_change(UserId id, UserRole role,
                                                 unsigned level,
                                                 UserId a) const {
  require_superadmin(a);
  auto old = user(id);
  RoleInput(role, level);
  if (old.role == UserRole::SuperAdmin)
    throw std::invalid_argument("Cannot change the superadministrator");
  ChangeImpact out;
  out.revision = revision();
  if (old.role == role && old.admin_level == level) return out;
  auto ids = state_->grants.ids();
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const auto& g = state_->grants.get(ids.get(i));
    if (g.subject_id != id) continue;
    auto impact = state_->revocation(g.id);
    for (std::size_t j = 0; j < impact.grants.size(); ++j)
      if (!Contains(out.grants, impact.grants.get(j)))
        out.grants.append(impact.grants.get(j));
    for (std::size_t j = 0; j < impact.rules.size(); ++j)
      if (!Contains(out.rules, impact.rules.get(j)))
        out.rules.append(impact.rules.get(j));
  }
  return out;
}
void ProjectService::set_role(UserId id, UserRole role, unsigned level,
                              UserId a, std::uint64_t expected) {
  auto impact = preview_role_change(id, role, level, a);
  if (expected != revision())
    throw std::runtime_error("Project changed: refresh the preview");
  auto old = user(id);
  if (old.role == role && old.admin_level == level) return;
  auto next = state_->clone();
  next->apply_revocation(impact);
  auto& u = next->users.users_->get(id);
  u.role = role;
  u.admin_level = static_cast<std::uint16_t>(level);
  AuthorizationGraph::validate(*next);
  ++next->revision;
  state_.swap(next);
}
AccessRuleId ProjectService::upsert_rule(NodeId node_id, AccessRuleId id,
                                         const RuleRequest& r, UserId a,
                                         AdminGrantId source) {
  user(a);
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  auth.require_source(node_id, a, source, AdminCapability::ManageAccess);
  if (static_cast<unsigned>(r.read) > 2 || static_cast<unsigned>(r.write) > 2 ||
      !r.principal.id)
    throw std::invalid_argument("Invalid rule");
  if (r.principal.kind == PrincipalKind::User) {
    user(r.principal.id);
    if (!auth.higher(a, r.principal.id))
      throw AccessDenied("Can only change access for lower users");
  } else if (r.principal.kind == PrincipalKind::Group)
    group(r.principal.id);
  else
    throw std::invalid_argument("Invalid principal kind");
  const auto& old = state_->vfs.get_node_unchecked(node_id).access_policy.rules;
  std::size_t position = old.size();
  if (id) {
    for (std::size_t i = 0; i < old.size(); ++i)
      if (old.get(i).id == id) {
        position = i;
        break;
      }
    if (position == old.size()) throw std::out_of_range("Unknown local rule");
    if (!auth.can_edit(node_id, old.get(position), a))
      throw AccessDenied(
          "This rule belongs to a higher or equal administrator");
  }
  AccessRule value{
      id ? id : state_->new_rule_id(), r.principal, r.read, r.write, a, source};
  if (id) {
    const auto& v = old.get(position);
    if (v.principal == value.principal && v.read == value.read &&
        v.write == value.write && v.issuer_id == a &&
        v.source_grant_id == source)
      return id;
  }
  ArraySequence<AccessRule> prepared(old);
  if (id)
    prepared.get(position) = value;
  else
    prepared.append(value);
  state_->vfs.nodes_->get(node_id).access_policy.rules.swap(prepared);
  ++state_->revision;
  return value.id;
}
void ProjectService::remove_rule(NodeId node_id, AccessRuleId id, UserId a,
                                 AdminGrantId source) {
  user(a);
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  auth.require_source(node_id, a, source, AdminCapability::ManageAccess);
  auto found = state_->rule(id);
  if (found.first != node_id) throw std::out_of_range("Rule is not local");
  if (!auth.can_edit(node_id, found.second, a))
    throw AccessDenied("Cannot remove protected rule");
  ArraySequence<AccessRule> prepared(
      state_->vfs.get_node_unchecked(node_id).access_policy.rules);
  for (std::size_t i = 0; i < prepared.size(); ++i)
    if (prepared.get(i).id == id) {
      prepared.remove_at(i);
      break;
    }
  state_->vfs.nodes_->get(node_id).access_policy.rules.swap(prepared);
  ++state_->revision;
}
void ProjectService::set_inheritance(NodeId id, bool inherit, UserId a,
                                     AdminGrantId source) {
  user(a);
  AuthorizationService before(state_->users, state_->vfs, state_->grants);
  before.require_source(id, a, source, AdminCapability::ManageAccess);
  if (id == root_id() && inherit)
    throw std::invalid_argument("Root cannot inherit");
  if (state_->vfs.get_node_unchecked(id).access_policy.inherit_from_parent ==
      inherit)
    return;
  auto next = state_->clone();
  next->vfs.nodes_->get(id).access_policy.inherit_from_parent = inherit;
  if (!inherit && user(a).role != UserRole::SuperAdmin) {
    AuthorizationService after(next->users, next->vfs, next->grants);
    state_->vfs.nodes_->for_each([&](auto n, const auto&) {
      if (!before.covers(id, n)) return;
      auto old = before.inherited_rules(n, true);
      auto changed = after.inherited_rules(n, true);
      for (std::size_t i = 0; i < old.size(); ++i)
        if (!Contains(changed, old.get(i))) {
          auto [origin, rule] = state_->rule(old.get(i));
          if (!before.can_edit(origin, rule, a))
            throw AccessDenied("Cannot cut a protected inherited permission");
        }
    });
  }
  ++next->revision;
  state_.swap(next);
}
void ProjectService::move(NodeId id, NodeId parent, UserId a) {
  user(a);
  const auto& n = state_->vfs.get_node_unchecked(id);
  if (id == root_id()) throw std::invalid_argument("Cannot move root");
  require_write(id, a);
  require_write(n.parent_id, a);
  require_write(parent, a);
  if (n.parent_id == parent) return;
  AuthorizationService before(state_->users, state_->vfs, state_->grants);
  if (user(a).role != UserRole::SuperAdmin) {
    auto ids = state_->grants.ids();
    for (std::size_t i = 0; i < ids.size(); ++i)
      if (before.covers(id, state_->grants.get(ids.get(i)).scope_node_id))
        throw AccessDenied(
            "Only superadministrator may move an assignment scope");
  }
  auto next = state_->clone();
  next->vfs.move(id, parent);
  AuthorizationGraph::validate(*next);
  if (user(a).role != UserRole::SuperAdmin) {
    AuthorizationService after(next->users, next->vfs, next->grants);
    state_->vfs.nodes_->for_each([&](auto n_id, const auto&) {
      if (!before.covers(id, n_id)) return;
      for (bool only_allow : {false, true}) {
        auto old = before.inherited_rules(n_id, only_allow),
             changed = after.inherited_rules(n_id, only_allow);
        for (std::size_t side = 0; side < 2; ++side) {
          const auto& x = side ? changed : old;
          const auto& y = side ? old : changed;
          for (std::size_t i = 0; i < x.size(); ++i)
            if (!Contains(y, x.get(i))) {
              auto [origin, rule] = state_->rule(x.get(i));
              if (!before.can_edit(origin, rule, a))
                throw AccessDenied("Move changes a protected inherited rule");
            }
        }
      }
    });
  }
  ++next->revision;
  state_.swap(next);
}
void ProjectService::remove(NodeId id, bool recursive, UserId a) {
  user(a);
  if (id == root_id()) throw std::invalid_argument("Cannot remove root");
  const auto& n = state_->vfs.get_node_unchecked(id);
  if (!recursive && n.children.size())
    throw std::invalid_argument("Folder is not empty");
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  auto ids = state_->grants.ids();
  for (std::size_t i = 0; i < ids.size(); ++i)
    if (auth.covers(id, state_->grants.get(ids.get(i)).scope_node_id))
      throw AccessDenied("Revoke assignments before deleting their scopes");
  state_->vfs.nodes_->for_each([&](auto node_id, const FSNode& node) {
    if (!auth.covers(id, node_id)) return;
    require_write(node_id, a);
    require_write(node.parent_id, a);
    for (std::size_t j = 0; j < node.access_policy.rules.size(); ++j)
      if (!auth.can_edit(node_id, node.access_policy.rules.get(j), a))
        throw AccessDenied(
            "Cannot delete another administrator's local policy");
  });
  auto next = state_->clone();
  if (recursive)
    next->vfs.remove_recursive(id);
  else
    next->vfs.remove(id);
  ++next->revision;
  state_.swap(next);
}
AccessExplanation ProjectService::explain(NodeId id, UserId target,
                                          UserId a) const {
  user(a);
  user(target);
  if (a != target && !can_administer(id, a, AdminCapability::ManageAccess))
    throw AccessDenied("Manage access required to inspect another user");
  return AuthorizationService(state_->users, state_->vfs, state_->grants)
      .explain(id, target);
}
PolicyView ProjectService::policy_view(NodeId id, UserId a) const {
  if (!can_administer(id, a, AdminCapability::ManageAccess))
    throw AccessDenied("Manage access required");
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  PolicyView view;
  view.inherits =
      state_->vfs.get_node_unchecked(id).access_policy.inherit_from_parent;
  bool inherit_allow = true;
  for (NodeId cur = id; cur;
       cur = state_->vfs.get_node_unchecked(cur).parent_id) {
    const auto& policy = state_->vfs.get_node_unchecked(cur).access_policy;
    const auto& rules = policy.rules;
    for (std::size_t i = 0; i < rules.size(); ++i) {
      bool editable = cur == id && auth.can_edit(cur, rules.get(i), a);
      view.rules.append(
          {rules.get(i), cur, cur != id, editable,
           editable ? "Editable" : "Inherited or protected by author level",
           inherit_allow});
    }
    inherit_allow = inherit_allow && policy.inherit_from_parent;
  }
  return view;
}
ArraySequence<AdminGrant> ProjectService::admin_grants(UserId a) const {
  user(a);
  AuthorizationService auth(state_->users, state_->vfs, state_->grants);
  ArraySequence<AdminGrant> out;
  auto ids = state_->grants.ids();
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const auto& g = state_->grants.get(ids.get(i));
    if (g.subject_id == a || g.issuer_id == a ||
        (auth.can(g.scope_node_id, a,
                  AdminCapability::DelegateAdministration) &&
         auth.higher(a, g.subject_id)))
      out.append(g);
  }
  return out;
}

bool ProjectService::can_move(NodeId id, NodeId parent, UserId a) const {
  try {
    ProjectService candidate;
    candidate.state_ = state_->clone();
    candidate.move(id, parent, a);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}
