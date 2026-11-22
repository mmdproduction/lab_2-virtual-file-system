#pragma once
#include "Common/Guid.hpp"
using NodeId = Guid;
using UserId = Guid;
using GroupId = Guid;
using AdminGrantId = Guid;
using AccessRuleId = Guid;
inline constexpr NodeId kInvalidNodeId{};
inline constexpr UserId kInvalidUserId{};
inline constexpr GroupId kInvalidGroupId{};
inline constexpr AdminGrantId kInvalidAdminGrantId{};
inline constexpr AccessRuleId kInvalidAccessRuleId{};
