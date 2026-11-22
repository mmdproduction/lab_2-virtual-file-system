#include "AuthorizationGraph.hpp"

#include <limits>

#include "AuthorizationService.hpp"
#include "Dictionary/AVLDictionary.hpp"
#include "Project/ProjectState.hpp"
namespace {
void Check(bool good, const char* why) {
  if (!good) throw std::invalid_argument(why);
}
bool Name(const std::string& s) {
  return !s.empty() && s != "." && s != ".." &&
         s.find('/') == std::string::npos;
}
}  // namespace
void AuthorizationGraph::validate(const ProjectState& s) {
  try {
    AuthorizationService auth(s.users, s.vfs, s.grants);
    unsigned supers = 0;
    s.users.users_->for_each([&](auto id, const User& u) {
      Check(id && u.id == id && !u.name.empty(), "Invalid user");
      Check(u.role == UserRole::User || u.role == UserRole::Admin ||
                u.role == UserRole::SuperAdmin,
            "Unknown role");
      Check(u.role == UserRole::Admin ? IsValidAdminLevel(u.admin_level)
                                      : u.admin_level == 0,
            "Invalid level");
      if (u.role == UserRole::SuperAdmin) {
        ++supers;
        Check(id == s.users.admin_id_, "Superadministrator mismatch");
      }
      AvlDictionary<GroupId, bool> memberships;
      for (std::size_t i = 0; i < u.group_ids.size(); ++i) {
        auto g = u.group_ids.get(i);
        Check(g && s.users.groups_->contains_key(g), "Unknown group");
        memberships.add(g, true);
      }
    });
    Check(supers == 1, "Exactly one superadministrator required");
    s.users.groups_->for_each([&](auto id, const Group& g) {
      Check(id && g.id == id && !g.name.empty(), "Invalid group");
    });
    const auto& root = s.vfs.get_node_unchecked(s.vfs.root_id_);
    Check(root.type == FSType::Folder && root.name == "/" && !root.parent_id &&
              !root.access_policy.inherit_from_parent,
          "Invalid root");
    AvlDictionary<NodeId, bool> visited;
    AvlDictionary<AccessRuleId, bool> rule_ids;
    ArraySequence<NodeId> queue;
    queue.append(root.id);
    for (std::size_t i = 0; i < queue.size(); ++i) {
      auto id = queue.get(i);
      visited.add(id, true);
      const auto& n = s.vfs.get_node_unchecked(id);
      Check(id && n.id == id, "Invalid node ID");
      Check(n.type == FSType::Folder || n.type == FSType::File, "Invalid type");
      Check(n.type == FSType::Folder ? n.physical_path.empty()
                                     : !n.physical_path.empty(),
            "Invalid physical path");
      Check(static_cast<unsigned>(n.file_icon) <= 5, "Invalid icon");
      Check(n.type == FSType::File || n.file_icon == FileIcon::Default,
            "Folder icon invalid");
      Check(n.type == FSType::Folder || n.children.size() == 0,
            "File has children");
      if (id != root.id) Check(Name(n.name), "Invalid name");
      AvlDictionary<std::string, bool> sibling_names;
      for (std::size_t j = 0; j < n.children.size(); ++j) {
        auto child = n.children.get(j);
        const auto& c = s.vfs.get_node_unchecked(child);
        Check(c.parent_id == id, "Invalid parent");
        sibling_names.add(c.name, true);
        queue.append(child);
      }
      for (std::size_t j = 0; j < n.access_policy.rules.size(); ++j) {
        const auto& r = n.access_policy.rules.get(j);
        Check(r.id && r.principal.id, "Invalid ACL ID");
        rule_ids.add(r.id, true);
        Check(static_cast<unsigned>(r.read) <= 2 &&
                  static_cast<unsigned>(r.write) <= 2,
              "Invalid decision");
        Check(r.principal.kind == PrincipalKind::User ||
                  r.principal.kind == PrincipalKind::Group,
              "Invalid principal kind");
        if (r.principal.kind == PrincipalKind::User)
          s.users.get_user(r.principal.id);
        else
          s.users.get_group(r.principal.id);
        Check(auth.rule_valid(r, id), "ACL source outside scope or invalid");
      }
    }
    Check(visited.size() == s.vfs.nodes_->size(), "Disconnected tree");
    AvlDictionary<NodeId, bool> indexed;
    s.vfs.file_index_.dictionary_->for_each([&](const std::string& name,
                                                const auto& bucket) {
      Check(!name.empty() && bucket.size(), "Empty index bucket");
      for (std::size_t i = 0; i < bucket.size(); ++i) {
        auto id = bucket.get(i);
        const auto& n = s.vfs.get_node_unchecked(id);
        Check(n.type == FSType::File && n.name == name, "Invalid index entry");
        indexed.add(id, true);
      }
    });
    s.vfs.nodes_->for_each([&](auto id, const FSNode& n) {
      if (n.type == FSType::File)
        Check(indexed.contains_key(id), "Unindexed file");
    });
    s.grants.grants_->for_each([&](auto id, const AdminGrant& g) {
      Check(id == g.id && auth.grant_valid(g), "Invalid delegation graph");
    });
  } catch (const std::out_of_range&) {
    throw std::invalid_argument("Unknown project reference");
  }
}
