#pragma once
#include <cstdint>

#include "VirtualFileSystem/Identifiers.hpp"
enum class UserRole : std::uint8_t { User, Admin, SuperAdmin };
enum class AdminCapability : std::uint8_t {
  ReadContent = 1,
  WriteContent = 2,
  ManageAccess = 4,
  DelegateAdministration = 8
};
enum class PermissionDecision : std::uint8_t { Unset, Allow, Deny };
enum class PrincipalKind : std::uint8_t { User, Group };
struct Principal {
  PrincipalKind kind = PrincipalKind::User;
  Guid id{};
  bool operator==(const Principal&) const = default;
};
struct AdminGrant {
  AdminGrantId id = {};
  UserId subject_id = {};
  NodeId scope_node_id = {};
  std::uint8_t capabilities = 0;
  UserId issuer_id = {};
  AdminGrantId parent_grant_id = {};
};
inline bool HasCapability(std::uint8_t mask, AdminCapability capability) {
  return (mask & static_cast<std::uint8_t>(capability)) != 0;
}
inline bool IsValidCapabilities(unsigned mask) { return mask <= 15; }
inline bool IsValidAdminLevel(unsigned level) {
  return level >= 1 && level <= 255;
}
