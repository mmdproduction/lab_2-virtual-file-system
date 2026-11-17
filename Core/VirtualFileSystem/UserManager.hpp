#pragma once

#include <memory>

#include "AccessTypes.hpp"
#include "Dictionary/IDictionary.hpp"

using UserDictionary = IDictionary<UserId, User>;
using GroupDictionary = IDictionary<GroupId, Group>;

class UserManager {
private:
    std::unique_ptr<UserDictionary> users_;
    std::unique_ptr<GroupDictionary> groups_;

    UserId admin_id_ = kInvalidUserId;
    UserId next_user_id_ = 1;
    GroupId next_group_id_ = 1;

    void require_admin(UserId actor_id) const;
public:
    UserManager(std::unique_ptr<UserDictionary> users, std::unique_ptr<GroupDictionary> groups);

    UserId admin_id() const noexcept;

    const User& get_user(UserId id) const;
    const Group& get_group(GroupId id) const;

    UserId create_user(const std::string& name, UserId actor_id);
    GroupId create_group(const std::string& name, UserId actor_id);

    void add_user_to_group(UserId user_id, GroupId group_id, UserId actor_id);

    void remove_user_from_group(UserId user_id, GroupId group_id, UserId actor_id);
};
