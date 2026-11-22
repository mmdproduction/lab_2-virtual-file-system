#pragma once
#include <memory>

#include "AuthorizationTypes.hpp"
#include "Dictionary/IDictionary.hpp"
#include "Sequence/ArraySequence/ArraySequence.hpp"
class AdminGrantStore {
  friend class ProjectService;
  friend class ProjectState;
  friend class AuthorizationService;
  friend class AuthorizationGraph;
  friend class ProjectCodec;
  std::unique_ptr<IDictionary<AdminGrantId, AdminGrant>> grants_;

 public:
  AdminGrantStore();
  const AdminGrant& get(AdminGrantId id) const;
  ArraySequence<AdminGrantId> ids() const;
};
