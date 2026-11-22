#pragma once
#include "Authorization/AuthorizationTypes.hpp"
#include "Sequence/ArraySequence/ArraySequence.hpp"
struct AccessRights {
  bool read = false;
  bool write = false;
};
struct AccessRule {
  AccessRuleId id = 0;
  Principal principal;
  PermissionDecision read = PermissionDecision::Unset;
  PermissionDecision write = PermissionDecision::Unset;
  UserId issuer_id = 0;
  AdminGrantId source_grant_id = 0;
};
struct AccessPolicy {
  bool inherit_from_parent = true;
  ArraySequence<AccessRule> rules;
};
