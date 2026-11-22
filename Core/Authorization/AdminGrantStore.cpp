#include "AdminGrantStore.hpp"

#include <utility>

#include "Dictionary/AVLDictionary.hpp"
AdminGrantStore::AdminGrantStore()
    : grants_(std::make_unique<AvlDictionary<AdminGrantId, AdminGrant>>()) {}
const AdminGrant& AdminGrantStore::get(AdminGrantId id) const {
  return std::as_const(*grants_).get(id);
}
ArraySequence<AdminGrantId> AdminGrantStore::ids() const {
  ArraySequence<AdminGrantId> result;
  grants_->for_each([&](auto id, const auto&) { result.append(id); });
  return result;
}
