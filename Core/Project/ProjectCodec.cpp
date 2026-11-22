#include "ProjectCodec.hpp"

#include <charconv>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

#include "Authorization/AuthorizationGraph.hpp"
#include "Dictionary/AVLDictionary.hpp"
#include "ProjectService.hpp"
#include "ProjectState.hpp"
namespace {
constexpr std::size_t kMaxBytes = 64 * 1024 * 1024;
class Reader {
  std::istringstream in_;
  std::size_t length_;

 public:
  explicit Reader(const std::string& s) : in_(s), length_(s.size()) {
    in_.imbue(std::locale::classic());
  }
  std::string token() {
    std::string t;
    if (!(in_ >> t)) throw std::runtime_error("Incomplete project");
    return t;
  }
  void expect(const char* t) {
    if (token() != t) throw std::runtime_error("Unexpected project token");
  }
  std::uint64_t number() {
    auto t = token();
    std::uint64_t v = 0;
    auto result = std::from_chars(t.data(), t.data() + t.size(), v);
    if (result.ec != std::errc{} || result.ptr != t.data() + t.size())
      throw std::runtime_error("Expected unsigned decimal integer");
    return v;
  }
  std::uint64_t bounded(std::uint64_t max) {
    auto n = number();
    if (n > max) throw std::runtime_error("Number out of range");
    return n;
  }
  std::size_t count() { return static_cast<std::size_t>(bounded(length_)); }
  bool flag() { return bounded(1) != 0; }
  std::string quoted() {
    in_ >> std::ws;
    if (in_.peek() != '"') throw std::runtime_error("Expected quoted string");
    std::string v;
    if (!(in_ >> std::quoted(v))) throw std::runtime_error("Incomplete string");
    return v;
  }
  std::string block(const char* label) {
    expect(label);
    auto n = bounded(kMaxBytes);
    if (in_.get() != '\n') throw std::runtime_error("Invalid block header");
    std::string s(static_cast<std::size_t>(n), '\0');
    in_.read(s.data(), static_cast<std::streamsize>(n));
    if (static_cast<std::uint64_t>(in_.gcount()) != n)
      throw std::runtime_error("Truncated project block");
    return s;
  }
  void end() {
    in_ >> std::ws;
    if (!in_.eof()) throw std::runtime_error("Trailing project data");
  }
};
std::string Block(const char* label, const std::string& s) {
  return std::string(label) + " " + std::to_string(s.size()) + "\n" + s;
}
}  // namespace
std::string ProjectCodec::encode(const ProjectService& p) {
  const auto& s = *p.state_;
  AuthorizationGraph::validate(s);
  std::ostringstream users, vfs, auth;
  for (auto* out : {&users, &vfs, &auth}) {
    out->imbue(std::locale::classic());
    out->exceptions(std::ios::badbit | std::ios::failbit);
  }
  users << "USER_MANAGER 3\nSUPERADMIN " << s.users.admin_id_
        << "\nGROUPS " << s.users.groups_->size()
        << '\n';
  s.users.groups_->for_each([&](auto id, const Group& g) {
    users << "GROUP " << id << ' ' << std::quoted(g.name) << '\n';
  });
  users << "USERS " << s.users.users_->size() << '\n';
  s.users.users_->for_each([&](auto id, const User& u) {
    users << "USER " << id << ' ' << std::quoted(u.name) << ' '
          << static_cast<unsigned>(u.role) << ' ' << u.admin_level << ' '
          << u.group_ids.size();
    for (std::size_t i = 0; i < u.group_ids.size(); ++i)
      users << ' ' << u.group_ids.get(i);
    users << '\n';
  });
  vfs << "VFS 5\nROOT " << s.vfs.root_id_ << "\nNODES " << s.vfs.nodes_->size() << '\n';
  ArraySequence<NodeId> ids;
  ids.append(s.vfs.root_id_);
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const auto& n = s.vfs.get_node_unchecked(ids.get(i));
    vfs << n.id << ' ' << (n.type == FSType::File ? 'F' : 'D') << ' '
        << n.parent_id << ' ' << std::quoted(n.name) << ' '
        << std::quoted(n.physical_path) << '\n';
    vfs << "POLICY " << n.access_policy.inherit_from_parent << ' '
        << n.access_policy.rules.size() << '\n';
    for (std::size_t j = 0; j < n.access_policy.rules.size(); ++j) {
      const auto& r = n.access_policy.rules.get(j);
      vfs << "RULE " << r.id << ' ' << static_cast<unsigned>(r.principal.kind)
          << ' ' << r.principal.id << ' ' << static_cast<unsigned>(r.read)
          << ' ' << static_cast<unsigned>(r.write) << ' ' << r.issuer_id << ' '
          << r.source_grant_id << '\n';
    }
    vfs << "ICON " << static_cast<unsigned>(n.file_icon) << '\n';
    for (std::size_t j = 0; j < n.children.size(); ++j)
      ids.append(n.children.get(j));
  }
  vfs << "INDEX " << s.vfs.file_index_.dictionary_->size() << '\n';
  s.vfs.file_index_.dictionary_->for_each(
      [&](const auto& name, const auto& bucket) {
        vfs << "BUCKET " << std::quoted(name) << ' ' << bucket.size();
        for (std::size_t j = 0; j < bucket.size(); ++j)
          vfs << ' ' << bucket.get(j);
        vfs << '\n';
      });
  auth << "AUTHORIZATION 2\nGRANTS "
       << s.grants.grants_->size() << '\n';
  s.grants.grants_->for_each([&](auto id, const AdminGrant& g) {
    auth << "GRANT " << id << ' ' << g.subject_id << ' ' << g.scope_node_id
         << ' ' << static_cast<unsigned>(g.capabilities) << ' ' << g.issuer_id
         << ' ' << g.parent_grant_id << '\n';
  });
  auto result = "VFS_PROJECT 3\n" + Block("USERS", users.str()) +
                Block("VFS", vfs.str()) + Block("AUTH", auth.str());
  if (result.size() > kMaxBytes)
    throw std::length_error("Project exceeds 64 MiB");
  return result;
}
std::unique_ptr<ProjectService> ProjectCodec::decode(const std::string& bytes) {
  if (bytes.size() > kMaxBytes)
    throw std::length_error("Project exceeds 64 MiB");
  Reader envelope(bytes);
  envelope.expect("VFS_PROJECT");
  auto version = envelope.number();
  if (version != 1 && version != 2 && version != 3)
    throw std::runtime_error("Unsupported project version");
  auto users_text = envelope.block("USERS"), vfs_text = envelope.block("VFS");
  std::string auth_text;
  if (version >= 2) auth_text = envelope.block("AUTH");
  envelope.end();
  auto project = std::make_unique<ProjectService>();
  auto& s = *project->state_;
  s.users.users_->clear();
  s.vfs.nodes_->clear();
  s.migrated = version < 3;
  enum Kind { UserKey, GroupKey, NodeKey, GrantKey, RuleKey };
  AvlDictionary<std::uint64_t, Guid> legacy_maps[5];
  AvlDictionary<Guid, bool> generated;
  std::uint64_t maximum[5]{}, next[5]{};
  auto read_id = [&](Reader& input, Kind kind, bool definition = false) {
    if (version == 3) return GuidFromString(input.token());
    auto raw = input.number();
    if (definition) maximum[kind] = std::max(maximum[kind], raw);
    if (!raw) return Guid{};
    auto& map = legacy_maps[kind];
    if (!map.contains_key(raw)) {
      auto id = GenerateUniqueGuid([&](Guid value) { return generated.contains_key(value); });
      generated.add(id, true);
      map.add(raw, id);
    }
    return map.get(raw);
  };
  Reader users(users_text);
  users.expect("USER_MANAGER");
  if (users.number() != version)
    throw std::runtime_error("Mismatched registry version");
  users.expect(version == 1 ? "ADMIN" : "SUPERADMIN");
  s.users.admin_id_ = read_id(users, UserKey);
  if (version < 3) {
    users.expect("NEXT_USER_ID"); next[UserKey] = users.number();
    users.expect("NEXT_GROUP_ID"); next[GroupKey] = users.number();
  }
  users.expect("GROUPS");
  auto ng = users.count();
  for (std::size_t i = 0; i < ng; ++i) {
    users.expect("GROUP");
    Group g;
    g.id = read_id(users, GroupKey, true);
    g.name = users.quoted();
    s.users.groups_->add(g.id, g);
  }
  users.expect("USERS");
  auto nu = users.count();
  ArraySequence<UserId> old_admins;
  for (std::size_t i = 0; i < nu; ++i) {
    users.expect("USER");
    User u;
    u.id = read_id(users, UserKey, true);
    u.name = users.quoted();
    if (version == 1) {
      bool admin = users.flag();
      u.role = admin ? (u.id == s.users.admin_id_ ? UserRole::SuperAdmin
                                                  : UserRole::Admin)
                     : UserRole::User;
      u.admin_level = u.role == UserRole::Admin ? 1 : 0;
      if (u.role == UserRole::Admin) old_admins.append(u.id);
    } else {
      u.role = static_cast<UserRole>(users.bounded(2));
      u.admin_level = static_cast<std::uint16_t>(users.bounded(255));
    }
    auto groups = users.count();
    for (std::size_t j = 0; j < groups; ++j) u.group_ids.append(read_id(users, GroupKey));
    s.users.users_->add(u.id, u);
  }
  users.end();
  Reader vfs(vfs_text);
  vfs.expect("VFS");
  auto vv = vfs.number();
  if ((version == 1 && (vv < 1 || vv > 3)) || (version == 2 && vv != 4) || (version == 3 && vv != 5))
    throw std::runtime_error("Mismatched VFS version");
  vfs.expect("ROOT");
  s.vfs.root_id_ = read_id(vfs, NodeKey);
  if (version < 3) { vfs.expect("NEXT_ID"); next[NodeKey] = vfs.number(); }
  vfs.expect("NODES");
  auto nn = vfs.count();
  ArraySequence<NodeId> records;
  for (std::size_t i = 0; i < nn; ++i) {
    FSNode n;
    n.id = read_id(vfs, NodeKey, true);
    auto type = vfs.token();
    if (type != "F" && type != "D")
      throw std::runtime_error("Invalid node type");
    n.type = type == "F" ? FSType::File : FSType::Folder;
    n.parent_id = read_id(vfs, NodeKey);
    n.name = vfs.quoted();
    n.physical_path = vfs.quoted();
    n.access_policy.inherit_from_parent = n.id != s.vfs.root_id_;
    if (vv >= 2) {
      vfs.expect("POLICY");
      n.access_policy.inherit_from_parent = vfs.flag();
      auto rules = vfs.count();
      AvlDictionary<GroupId, bool> legacy_groups;
      for (std::size_t j = 0; j < rules; ++j) {
        vfs.expect("RULE");
        AccessRule r;
        if (vv < 4) {
          r.id = GenerateUniqueGuid([&](Guid value) { return generated.contains_key(value); });
          generated.add(r.id, true);
          r.principal = {PrincipalKind::Group, read_id(vfs, GroupKey)};
          legacy_groups.add(r.principal.id, true);
          r.read = vfs.flag() ? PermissionDecision::Allow
                              : PermissionDecision::Unset;
          r.write = vfs.flag() ? PermissionDecision::Allow
                               : PermissionDecision::Unset;
          r.issuer_id = s.users.admin_id_;
        } else {
          r.id = read_id(vfs, RuleKey, true);
          r.principal.kind = static_cast<PrincipalKind>(vfs.bounded(1));
          r.principal.id = read_id(vfs, r.principal.kind == PrincipalKind::User ? UserKey : GroupKey);
          r.read = static_cast<PermissionDecision>(vfs.bounded(2));
          r.write = static_cast<PermissionDecision>(vfs.bounded(2));
          r.issuer_id = read_id(vfs, UserKey);
          r.source_grant_id = read_id(vfs, GrantKey);
        }
        n.access_policy.rules.append(r);
      }
    }
    if (vv >= 3) {
      vfs.expect("ICON");
      n.file_icon = static_cast<FileIcon>(vfs.bounded(5));
    }
    s.vfs.nodes_->add(n.id, n);
    records.append(n.id);
  }
  if (vv >= 4) {
    vfs.expect("INDEX");
    auto count = vfs.count();
    for (std::size_t i = 0; i < count; ++i) {
      vfs.expect("BUCKET");
      auto name = vfs.quoted();
      auto n = vfs.count();
      ArraySequence<NodeId> bucket;
      for (std::size_t j = 0; j < n; ++j) bucket.append(read_id(vfs, NodeKey));
      s.vfs.file_index_.dictionary_->add(name, bucket);
    }
  }
  vfs.end();
  for (std::size_t i = 0; i < records.size(); ++i) {
    auto id = records.get(i);
    auto& n = s.vfs.nodes_->get(id);
    if (id != s.vfs.root_id_) {
      auto& parent = s.vfs.nodes_->get(n.parent_id);
      if (parent.type != FSType::Folder)
        throw std::invalid_argument("File is parent");
      parent.children.append(id);
    }
    if (vv < 4 && n.type == FSType::File) s.vfs.file_index_.add(n.name, n.id);
  }
  if (version >= 2) {
    Reader auth(auth_text);
    auth.expect("AUTHORIZATION");
    if (auth.number() != (version == 3 ? 2U : 1U))
      throw std::runtime_error("Unknown authorization version");
    if (version < 3) {
      auth.expect("NEXT_GRANT_ID"); next[GrantKey] = auth.number();
      auth.expect("NEXT_RULE_ID"); next[RuleKey] = auth.number();
    }
    auth.expect("GRANTS");
    auto count = auth.count();
    for (std::size_t i = 0; i < count; ++i) {
      auth.expect("GRANT");
      AdminGrant g;
      g.id = read_id(auth, GrantKey, true);
      g.subject_id = read_id(auth, UserKey);
      g.scope_node_id = read_id(auth, NodeKey);
      g.capabilities = static_cast<std::uint8_t>(auth.bounded(15));
      g.issuer_id = read_id(auth, UserKey);
      g.parent_grant_id = read_id(auth, GrantKey);
      s.grants.grants_->add(g.id, g);
    }
    auth.end();
  } else
    for (std::size_t i = 0; i < old_admins.size(); ++i) {
      AdminGrant g{GenerateUniqueGuid([&](Guid id) { return s.grants.grants_->contains_key(id); }),
                   old_admins.get(i), s.vfs.root_id_, 7, s.users.admin_id_, {}};
      s.grants.grants_->add(g.id, g);
    }
  if (version < 3) {
    const unsigned limit = version == 1 ? 3 : 5;
    for (unsigned k = 0; k < limit; ++k)
      if (next[k] && next[k] <= maximum[k])
        throw std::invalid_argument("Legacy counter reuses ID");
  }
  AuthorizationGraph::validate(s);
  return project;
}
std::string ProjectService::save(UserId actor) const {
  require_superadmin(actor);
  return ProjectCodec::encode(*this);
}
std::unique_ptr<ProjectService> ProjectService::load(const std::string& bytes,
                                                     UserId actor) const {
  require_superadmin(actor);
  return ProjectCodec::decode(bytes);
}
