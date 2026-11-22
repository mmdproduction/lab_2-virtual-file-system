#pragma once
#include <cstdint>
using NodeId = std::uint64_t;
using UserId = std::uint64_t;
using GroupId = std::uint64_t;
using AdminGrantId = std::uint64_t;
using AccessRuleId = std::uint64_t;
inline constexpr NodeId kInvalidNodeId = 0;
inline constexpr UserId kInvalidUserId = 0;
inline constexpr GroupId kInvalidGroupId = 0;
inline constexpr AdminGrantId kInvalidAdminGrantId = 0;
inline constexpr AccessRuleId kInvalidAccessRuleId = 0;
