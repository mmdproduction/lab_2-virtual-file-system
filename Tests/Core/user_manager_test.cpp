#include "test_support.hpp"

namespace core_test {
class UserManagerTest : public ::testing::Test {
 protected:
  UserManager users = MakeUserManager();
  UserId admin = users.admin_id();
};
TEST(UserManagerConstructionTest, RejectsNullOrNonemptyStorage) {
  EXPECT_THROW(UserManager(nullptr, MakeGroups()), std::invalid_argument);
  EXPECT_THROW(UserManager(MakeUsers(), nullptr), std::invalid_argument);
  auto user_store = MakeUsers();
  user_store->add(50, Admin());
  EXPECT_THROW(UserManager(std::move(user_store), MakeGroups()),
               std::invalid_argument);
  auto group_store = MakeGroups();
  group_store->add(50, Group{50, "old"});
  EXPECT_THROW(UserManager(MakeUsers(), std::move(group_store)),
               std::invalid_argument);
}
TEST_F(UserManagerTest, StartsWithOneAdministratorAndNoGroups) {
  const auto& actor = users.get_user(admin);
  EXPECT_NE(admin, kInvalidUserId);
  EXPECT_TRUE(actor.is_admin);
  EXPECT_EQ(actor.name, "admin");
  ExpectSequence<UserId>(users.user_ids(), {admin});
  EXPECT_EQ(users.group_ids().size(), 0U);
}
TEST_F(UserManagerTest, CreatesDistinctStableIdsAndOrdinaryUsers) {
  const auto first = users.create_user("same", admin);
  const auto second = users.create_user("same", admin);
  EXPECT_NE(first, second);
  EXPECT_FALSE(users.get_user(first).is_admin);
  EXPECT_EQ(users.get_user(first).group_ids.size(), 0U);
  const auto a = users.create_group("artists", admin);
  const auto b = users.create_group("artists", admin);
  EXPECT_NE(a, b);
  ExpectSequence<GroupId>(users.group_ids(), {a, b});
  ExpectSequence<UserId>(users.user_ids(), {admin, first, second});
}
TEST_F(UserManagerTest, EmptyNamesAreRejectedWithoutConsumingIds) {
  EXPECT_THROW(users.create_user("", admin), std::invalid_argument);
  EXPECT_THROW(users.create_group("", admin), std::invalid_argument);
  EXPECT_EQ(users.create_user("valid", admin), admin + 1);
  EXPECT_EQ(users.create_group("valid", admin), 1U);
}
TEST_F(UserManagerTest, RenamesPreserveIdsPrivilegesAndMemberships) {
  auto id = users.create_user("old", admin);
  auto group = users.create_group("oldgroup", admin);
  users.add_user_to_group(id, group, admin);
  users.rename_user(id, "новое имя", admin);
  users.rename_group(group, "newgroup", admin);
  EXPECT_EQ(users.get_user(id).name, "новое имя");
  EXPECT_FALSE(users.get_user(id).is_admin);
  ExpectSequence<GroupId>(users.get_user(id).group_ids, {group});
  EXPECT_EQ(users.get_group(group).name, "newgroup");
  users.rename_user(admin, "root", admin);
  EXPECT_TRUE(users.get_user(admin).is_admin);
}
TEST_F(UserManagerTest, FailedRenamesDoNotChangeOriginalNames) {
  auto id = users.create_user("original", admin);
  auto group = users.create_group("original", admin);
  EXPECT_THROW(users.rename_user(id, "", admin), std::invalid_argument);
  EXPECT_THROW(users.rename_group(group, "", admin), std::invalid_argument);
  EXPECT_THROW(users.rename_user(999, "new", admin), std::out_of_range);
  EXPECT_THROW(users.rename_group(999, "new", admin), std::out_of_range);
  EXPECT_EQ(users.get_user(id).name, "original");
  EXPECT_EQ(users.get_group(group).name, "original");
}
TEST_F(UserManagerTest, MembershipAddAndRemoveAreIdempotent) {
  auto id = users.create_user("member", admin);
  auto first = users.create_group("first", admin);
  auto second = users.create_group("second", admin);
  users.add_user_to_group(id, first, admin);
  users.add_user_to_group(id, second, admin);
  users.add_user_to_group(id, first, admin);
  ExpectSequence<GroupId>(users.get_user(id).group_ids, {first, second});
  users.remove_user_from_group(id, first, admin);
  users.remove_user_from_group(id, first, admin);
  ExpectSequence<GroupId>(users.get_user(id).group_ids, {second});
}
TEST_F(UserManagerTest, MissingUsersAndGroupsDoNotChangeMemberships) {
  auto id = users.create_user("member", admin);
  auto group = users.create_group("group", admin);
  EXPECT_THROW(users.add_user_to_group(id, 999, admin), std::out_of_range);
  EXPECT_THROW(users.add_user_to_group(999, group, admin), std::out_of_range);
  EXPECT_THROW(users.remove_user_from_group(id, 999, admin), std::out_of_range);
  EXPECT_EQ(users.get_user(id).group_ids.size(), 0U);
  EXPECT_THROW(users.get_user(999), std::out_of_range);
  EXPECT_THROW(users.get_group(999), std::out_of_range);
}
TEST_F(UserManagerTest, OrdinaryUserCannotAdministerOrExportRegistry) {
  auto id = users.create_user("member", admin);
  auto group = users.create_group("group", admin);
  const auto before = SaveUsers(users);
  EXPECT_THROW(users.create_user("forbidden", id), std::runtime_error);
  EXPECT_THROW(users.create_group("forbidden", id), std::runtime_error);
  EXPECT_THROW(users.rename_user(admin, "forbidden", id), std::runtime_error);
  EXPECT_THROW(users.rename_group(group, "forbidden", id), std::runtime_error);
  EXPECT_THROW(users.add_user_to_group(id, group, id), std::runtime_error);
  EXPECT_THROW(users.remove_user_from_group(id, group, id), std::runtime_error);
  std::ostringstream output;
  EXPECT_THROW(users.save(output, id), std::runtime_error);
  EXPECT_TRUE(output.str().empty());
  EXPECT_EQ(SaveUsers(users), before);
}
TEST_F(UserManagerTest, UnknownActorCannotAdministerRegistry) {
  EXPECT_THROW(users.create_user("bad", 0), std::out_of_range);
  EXPECT_THROW(users.create_group("bad", 999), std::out_of_range);
  ExpectSequence<UserId>(users.user_ids(), {admin});
}
TEST_F(UserManagerTest, EnumeratedIdListsAreIndependentCopies) {
  auto ids = users.user_ids();
  ids.clear();
  EXPECT_EQ(users.user_ids().size(), 1U);
  auto group = users.create_group("g", admin);
  auto groups = users.group_ids();
  groups.get(0) = 999;
  ExpectSequence<GroupId>(users.group_ids(), {group});
}
}  // namespace core_test
