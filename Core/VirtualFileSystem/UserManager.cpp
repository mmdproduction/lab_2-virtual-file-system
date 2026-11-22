#include "UserManager.hpp"

#include <cstddef>
#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

UserManager::UserManager(std::unique_ptr<UserDictionary> users,
                         std::unique_ptr<GroupDictionary> groups)
    : users_(std::move(users)), groups_(std::move(groups)) {
  if (!users_ || !groups_) {
    throw std::invalid_argument("User and group dictionaries must not be null");
  }
  if (users_->size() != 0 || groups_->size() != 0) {
    throw std::invalid_argument("User and group dictionaries must be empty");
  }

  User admin;
  admin.id = next_user_id_;
  admin.name = "admin";
  admin.role = UserRole::SuperAdmin;

  users_->add(admin.id, admin);
  admin_id_ = admin.id;
  ++next_user_id_;
}

UserId UserManager::admin_id() const noexcept { return admin_id_; }

const User& UserManager::get_user(UserId id) const {
  const UserDictionary& dictionary = *users_;
  return dictionary.get(id);
}

const Group& UserManager::get_group(GroupId id) const {
  const GroupDictionary& dictionary = *groups_;
  return dictionary.get(id);
}

ArraySequence<UserId> UserManager::user_ids() const {
  ArraySequence<UserId> result;
  users_->for_each(
      [&result](const UserId& id, const User&) { result.append(id); });
  return result;
}

ArraySequence<GroupId> UserManager::group_ids() const {
  ArraySequence<GroupId> result;
  groups_->for_each(
      [&result](const GroupId& id, const Group&) { result.append(id); });
  return result;
}

void UserManager::rename_user(UserId id, const std::string& name) {
  User& user = users_->get(id);
  if (name.empty()) throw std::invalid_argument("User name must not be empty");
  std::string prepared(name);
  user.name.swap(prepared);
}

void UserManager::rename_group(GroupId id, const std::string& name) {
  Group& group = groups_->get(id);
  if (name.empty()) throw std::invalid_argument("Group name must not be empty");
  std::string prepared(name);
  group.name.swap(prepared);
}

UserId UserManager::create_user(const std::string& name) {
  if (name.empty()) {
    throw std::invalid_argument("User name must not be empty");
  }
  if (next_user_id_ == kInvalidUserId) {
    throw std::overflow_error("User IDs exhausted");
  }

  User user;
  user.id = next_user_id_;
  user.name = name;

  users_->add(user.id, user);
  ++next_user_id_;
  return user.id;
}

GroupId UserManager::create_group(const std::string& name) {
  if (name.empty()) {
    throw std::invalid_argument("Group name must not be empty");
  }
  if (next_group_id_ == kInvalidGroupId) {
    throw std::overflow_error("Group IDs exhausted");
  }

  Group group;
  group.id = next_group_id_;
  group.name = name;

  groups_->add(group.id, group);
  ++next_group_id_;
  return group.id;
}

void UserManager::add_user_to_group(UserId user_id, GroupId group_id) {
  get_group(group_id);

  User& user = users_->get(user_id);
  for (std::size_t i = 0; i < user.group_ids.size(); ++i) {
    if (user.group_ids.get(i) == group_id) {
      return;
    }
  }

  user.group_ids.append(group_id);
}

void UserManager::remove_user_from_group(UserId user_id, GroupId group_id) {
  get_group(group_id);

  User& user = users_->get(user_id);
  for (std::size_t i = 0; i < user.group_ids.size(); ++i) {
    if (user.group_ids.get(i) == group_id) {
      user.group_ids.remove_at(i);
      return;
    }
  }
}
