#pragma once

#include <cstdint>
#include <string>

#include "Sequence/ArraySequence/ArraySequence.hpp"

using UserId = std::uint64_t;
using GroupId = std::uint64_t;

inline constexpr UserId kInvalidUserId = 0;
inline constexpr GroupId kInvalidGroupId = 0;

struct User {
    UserId id = kInvalidUserId;
    std::string name;
    bool is_admin = false;
    ArraySequence<GroupId> group_ids;
};

struct Group {
    GroupId id = kInvalidGroupId;
    std::string name;
};