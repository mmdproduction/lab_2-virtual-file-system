#pragma once
#include <string>

#include "Authorization/AuthorizationTypes.hpp"
#include "Sequence/ArraySequence/ArraySequence.hpp"
struct User {
  UserId id = {};
  std::string name;
  UserRole role = UserRole::User;
  std::uint16_t admin_level = 0;
  ArraySequence<GroupId> group_ids;
};
struct Group {
  GroupId id = {};
  std::string name;
};
