#pragma once
#include <memory>
#include <string>

#include "AccessTypes.hpp"
#include "Dictionary/IDictionary.hpp"
using UserDictionary = IDictionary<UserId, User>;
using GroupDictionary = IDictionary<GroupId, Group>;
class UserManager {
  friend class ProjectService;
  friend class ProjectState;
  friend class AuthorizationService;
  friend class AuthorizationGraph;
  friend class ProjectCodec;
  std::unique_ptr<UserDictionary> users_;
  std::unique_ptr<GroupDictionary> groups_;
  UserId admin_id_ = 0, next_user_id_ = 1;
  GroupId next_group_id_ = 1;
  void rename_user(UserId, const std::string&);
  void rename_group(GroupId, const std::string&);
  UserId create_user(const std::string&);
  GroupId create_group(const std::string&);
  void add_user_to_group(UserId, GroupId);
  void remove_user_from_group(UserId, GroupId);

 public:
  UserManager(std::unique_ptr<UserDictionary>,
              std::unique_ptr<GroupDictionary>);
  UserId admin_id() const noexcept;
  const User& get_user(UserId) const;
  const Group& get_group(GroupId) const;
  ArraySequence<UserId> user_ids() const;
  ArraySequence<GroupId> group_ids() const;
};
