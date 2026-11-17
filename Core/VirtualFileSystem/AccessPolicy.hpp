#pragma once

#include "AccessTypes.hpp"
#include "Sequence/ArraySequence/ArraySequence.hpp"

struct AccessRights {
    bool read = false;
    bool write = false;
};

struct GroupAccessRule {
    GroupId group_id = kInvalidGroupId;
    AccessRights rights;
};

struct AccessPolicy {
    bool inherit_from_parent = true;
    ArraySequence<GroupAccessRule> rules;
};