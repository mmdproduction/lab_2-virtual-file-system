#include "ProjectState.hpp"

#include "Dictionary/AVLDictionary.hpp"
ProjectState::ProjectState()
    : users(std::make_unique<AvlDictionary<UserId, User>>(),
            std::make_unique<AvlDictionary<GroupId, Group>>()),
      vfs(std::make_unique<AvlDictionary<NodeId, FSNode>>(),
          std::make_unique<
              AvlDictionary<std::string, ArraySequence<NodeId>>>()) {}
std::unique_ptr<ProjectState> ProjectState::clone() const {
  auto c = std::make_unique<ProjectState>();
  c->users.users_->clear();
  users.users_->for_each(
      [&](auto k, const auto& v) { c->users.users_->add(k, v); });
  users.groups_->for_each(
      [&](auto k, const auto& v) { c->users.groups_->add(k, v); });
  c->users.admin_id_ = users.admin_id_;
  c->vfs.nodes_->clear();
  vfs.nodes_->for_each(
      [&](auto k, const auto& v) { c->vfs.nodes_->add(k, v); });
  vfs.file_index_.dictionary_->for_each([&](const auto& k, const auto& v) {
    c->vfs.file_index_.dictionary_->add(k, v);
  });
  c->vfs.root_id_ = vfs.root_id_;
  grants.grants_->for_each(
      [&](auto k, const auto& v) { c->grants.grants_->add(k, v); });
  c->revision = revision;
  c->migrated = migrated;
  return c;
}
#include "ProjectHelpers.hpp"
ChangeImpact ProjectState::revocation(AdminGrantId id) const {
  grants.get(id);
  ChangeImpact impact;
  impact.revision = revision;
  impact.grants.append(id);
  auto ids = grants.ids();
  for (std::size_t i = 0; i < impact.grants.size(); ++i)
    for (std::size_t j = 0; j < ids.size(); ++j) {
      const auto& g = grants.get(ids.get(j));
      if (g.parent_grant_id == impact.grants.get(i) &&
          !Contains(impact.grants, g.id))
        impact.grants.append(g.id);
    }
  vfs.nodes_->for_each([&](auto, const FSNode& n) {
    for (std::size_t i = 0; i < n.access_policy.rules.size(); ++i) {
      const auto& r = n.access_policy.rules.get(i);
      if (Contains(impact.grants, r.source_grant_id)) impact.rules.append(r.id);
    }
  });
  return impact;
}
void ProjectState::apply_revocation(const ChangeImpact& impact) {
  for (std::size_t i = 0; i < impact.grants.size(); ++i)
    grants.grants_->remove(impact.grants.get(i));
  auto ids = ArraySequence<NodeId>();
  vfs.nodes_->for_each([&](auto id, const auto&) { ids.append(id); });
  for (std::size_t i = 0; i < ids.size(); ++i) {
    auto& rules = vfs.nodes_->get(ids.get(i)).access_policy.rules;
    for (std::size_t j = rules.size(); j > 0; --j)
      if (Contains(impact.rules, rules.get(j - 1).id)) rules.remove_at(j - 1);
  }
}
std::pair<NodeId, AccessRule> ProjectState::rule(AccessRuleId id) const {
  std::pair<NodeId, AccessRule> out;
  vfs.nodes_->for_each([&](auto node, const FSNode& n) {
    for (std::size_t i = 0; i < n.access_policy.rules.size(); ++i)
      if (n.access_policy.rules.get(i).id == id)
        out = {node, n.access_policy.rules.get(i)};
  });
  if (!out.first) throw std::out_of_range("Unknown rule");
  return out;
}

AccessRuleId ProjectState::new_rule_id() const {
  return GenerateUniqueGuid([&](Guid id) {
    bool found = false;
    vfs.nodes_->for_each([&](auto, const FSNode& n) {
      for (std::size_t i = 0; i < n.access_policy.rules.size(); ++i)
        found |= n.access_policy.rules.get(i).id == id;
    });
    return found;
  });
}
