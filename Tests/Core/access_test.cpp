#include "test_support.hpp"

namespace core_test {
class AccessTest : public VfsTest {};
TEST_F(AccessTest, DefaultDenyAndAdminBypassApplyOnlyToExistingNodes) {
  auto file = File(root);
  ExpectRights(vfs.get_access_rights(file, reader), false, false);
  EXPECT_THROW(vfs.get_node(file, reader), std::runtime_error);
  ExpectRights(vfs.get_access_rights(file, admin), true, true);
  EXPECT_THROW(vfs.get_access_rights(999, admin), std::out_of_range);
  User invalid;
  EXPECT_THROW(vfs.get_access_rights(file, invalid), std::invalid_argument);
  invalid.is_admin = true;
  EXPECT_THROW(vfs.get_access_rights(file, invalid), std::invalid_argument);
}
TEST_F(AccessTest, GroupGrantsAreUnitedAndUnrelatedGroupsHaveNoEffect) {
  auto file = File(root);
  vfs.set_access_policy(
      file,
      Policy(false,
             {{10, {true, false}}, {20, {false, true}}, {99, {true, true}}}),
      admin);
  ExpectRights(vfs.get_access_rights(file, Member({10})), true, false);
  ExpectRights(vfs.get_access_rights(file, Member({20})), false, true);
  ExpectRights(vfs.get_access_rights(file, Member({10, 20})), true, true);
  ExpectRights(vfs.get_access_rights(file, Member({30})), false, false);
  ExpectRights(vfs.get_access_rights(file, Member({})), false, false);
}
TEST_F(AccessTest, InheritanceCanBeCutOffAtIntermediateFolder) {
  Grant(root, true, false);
  auto folder = vfs.create_directory(root, "folder", admin);
  auto file = File(folder);
  ExpectRights(vfs.get_access_rights(file, reader), true, false);
  vfs.set_access_policy(folder, Policy(false), admin);
  ExpectRights(vfs.get_access_rights(file, reader), false, false);
  Grant(folder, false, true);
  ExpectRights(vfs.get_access_rights(file, reader), false, true);
}
TEST_F(AccessTest, LocalFalseFlagsAreNotDenyRulesAgainstInheritedGrants) {
  Grant(root);
  auto file = File(root);
  vfs.set_access_policy(file, Policy(true, {{10, {false, false}}}), admin);
  ExpectRights(vfs.get_access_rights(file, reader), true, true);
}
TEST_F(AccessTest, ReadAndWriteAreIndependentPermissions) {
  auto file = File(root);
  Grant(file, true, false);
  EXPECT_EQ(vfs.get_node(file, reader).id, file);
  EXPECT_THROW(vfs.set_file_icon(file, FileIcon::Code, reader),
               std::runtime_error);
  Grant(root, false, true);
  Grant(file, false, true);
  EXPECT_THROW(vfs.get_node(file, reader), std::runtime_error);
  EXPECT_NO_THROW(vfs.set_file_icon(file, FileIcon::Code, reader));
  EXPECT_EQ(vfs.get_node(file, admin).file_icon, FileIcon::Code);
}
TEST_F(AccessTest, CoreSearchFiltersFileReadRightsAndDoesNotChangeIndex) {
  auto a = vfs.create_directory(root, "a", admin);
  auto b = vfs.create_directory(root, "b", admin);
  auto visible = File(a, "same");
  auto hidden = File(b, "same");
  Grant(visible, true, false);
  ExpectSequence<NodeId>(vfs.find_files("same", reader), {visible});
  ExpectSequence<NodeId>(vfs.find_files("same", admin), {visible, hidden});
  User invalid;
  EXPECT_THROW(vfs.find_files("same", invalid), std::invalid_argument);
}
TEST_F(AccessTest, OnlyAdminMaySetPolicyAndInvalidPoliciesLeavePreviousGrants) {
  auto file = File(root);
  Grant(file, true, false);
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.set_access_policy(file, Policy(false), reader),
               std::runtime_error);
  EXPECT_THROW(
      vfs.set_access_policy(file, Policy(false, {{0, {true, true}}}), admin),
      std::invalid_argument);
  EXPECT_THROW(
      vfs.set_access_policy(
          file, Policy(false, {{10, {true, true}}, {10, {false, false}}}),
          admin),
      std::invalid_argument);
  EXPECT_THROW(vfs.set_access_policy(root, Policy(true), admin),
               std::invalid_argument);
  EXPECT_EQ(SaveVfs(vfs), before);
}
TEST_F(AccessTest, PolicyIsCopiedInsteadOfAliasingCallerRules) {
  auto file = File(root);
  auto policy = Policy(false, {{10, {true, false}}});
  vfs.set_access_policy(file, policy, admin);
  policy.rules.get(0).rights.write = true;
  ExpectRights(vfs.get_access_rights(file, reader), true, false);
}
TEST_F(AccessTest, CreateAndAttachNeedParentWriteAndDeniedAttemptsAreAtomic) {
  Grant(root, true, false);
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.create_directory(root, "denied", reader),
               std::runtime_error);
  EXPECT_THROW(vfs.attach_file(root, "denied", original, reader),
               std::runtime_error);
  EXPECT_EQ(SaveVfs(vfs), before);
  Grant(root);
  EXPECT_NO_THROW(vfs.create_directory(root, "allowed", reader));
}
TEST_F(AccessTest, RenameNeedsWriteOnBothNodeAndParent) {
  auto file = File(root);
  Grant(root);
  Grant(file, true, false);
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.rename(file, "denied", reader), std::runtime_error);
  EXPECT_EQ(SaveVfs(vfs), before);
  Grant(file);
  Grant(root, true, false);
  const auto second = SaveVfs(vfs);
  EXPECT_THROW(vfs.rename(file, "denied", reader), std::runtime_error);
  EXPECT_EQ(SaveVfs(vfs), second);
  Grant(root);
  vfs.rename(file, "allowed", reader);
  ExpectSequence<NodeId>(vfs.find_files("allowed", reader), {file});
}
TEST_F(AccessTest, MoveNeedsWriteOnNodeOldParentAndNewParent) {
  Grant(root);
  auto old = vfs.create_directory(root, "old", admin);
  auto target = vfs.create_directory(root, "target", admin);
  auto file = File(old);
  for (NodeId locked : {file, old, target}) {
    Grant(file);
    Grant(old);
    Grant(target);
    Grant(locked, true, false);
    const auto before = SaveVfs(vfs);
    EXPECT_THROW(vfs.move(file, target, reader), std::runtime_error) << locked;
    EXPECT_EQ(SaveVfs(vfs), before);
  }
  Grant(target);
  vfs.move(file, target, reader);
  EXPECT_EQ(vfs.get_node(file, admin).parent_id, target);
}
TEST_F(AccessTest, MoveRecomputesInheritedRightsUnderNewParent) {
  auto old = vfs.create_directory(root, "old", admin);
  auto target = vfs.create_directory(root, "target", admin);
  auto file = File(old);
  Grant(old, true, false);
  Grant(target, false, true);
  ExpectRights(vfs.get_access_rights(file, reader), true, false);
  vfs.move(file, target, admin);
  ExpectRights(vfs.get_access_rights(file, reader), false, true);
}
TEST_F(AccessTest, RemoveNeedsWriteOnNodeAndParent) {
  auto file = File(root);
  Grant(root);
  Grant(file, true, false);
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.remove(file, reader), std::runtime_error);
  EXPECT_EQ(SaveVfs(vfs), before);
  Grant(file);
  Grant(root, true, false);
  const auto second = SaveVfs(vfs);
  EXPECT_THROW(vfs.remove(file, reader), std::runtime_error);
  EXPECT_EQ(SaveVfs(vfs), second);
}
TEST_F(AccessTest, RecursiveRemovePreflightsAllRightsBeforeDeletingAnything) {
  Grant(root);
  auto folder = vfs.create_directory(root, "folder", admin);
  auto first = File(folder, "first");
  auto last = File(folder, "last");
  Grant(last, true, false);
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.remove_recursive(folder, reader), std::runtime_error);
  EXPECT_EQ(SaveVfs(vfs), before);
  EXPECT_EQ(vfs.get_node(first, admin).parent_id, folder);
  EXPECT_EQ(ReadFile(original), "original contents");
}
}  // namespace core_test
