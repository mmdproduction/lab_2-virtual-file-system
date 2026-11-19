#include <limits>
#include <streambuf>

#include "test_support.hpp"

namespace core_test {
TEST_F(VfsTest, SaveLoadPreservesHierarchyPoliciesSearchAndDeletedIdHistory) {
  Grant(root, true, false);
  auto assets = vfs.create_directory(root, "папка \"x\" \\\n", admin);
  auto file = File(assets, "shared.txt");
  auto second = File(root, "shared.txt");
  auto deleted = vfs.create_directory(root, "deleted", admin);
  vfs.remove(deleted, admin);
  auto loaded = LoadVfs(SaveVfs(vfs));
  EXPECT_EQ(loaded.get_node(assets, admin).name, "папка \"x\" \\\n");
  EXPECT_EQ(loaded.get_node(file, admin).parent_id, assets);
  EXPECT_EQ(loaded.get_node(file, admin).physical_path, original);
  ExpectSequence<NodeId>(loaded.get_node(root, admin).children,
                         {assets, second});
  const auto matches = loaded.find_files("shared.txt", admin);
  ASSERT_EQ(matches.size(), 2U);
  EXPECT_TRUE((matches.get(0) == file && matches.get(1) == second) ||
              (matches.get(0) == second && matches.get(1) == file));
  ExpectRights(loaded.get_access_rights(file, reader), true, false);
  EXPECT_GT(loaded.create_directory(root, "new", admin), deleted);
  EXPECT_EQ(ReadFile(original), "original contents");
}
class IconRoundTripTest : public VfsTest,
                          public ::testing::WithParamInterface<FileIcon> {};
TEST_P(IconRoundTripTest, RetainsSelectedIconAndAppropriateFormatVersion) {
  auto file = File(root);
  vfs.set_file_icon(file, GetParam(), admin);
  const auto text = SaveVfs(vfs);
  EXPECT_EQ(text.substr(0, 6),
            GetParam() == FileIcon::Default ? "VFS 2\n" : "VFS 3\n");
  auto loaded = LoadVfs(text);
  EXPECT_EQ(loaded.get_node(file, admin).file_icon, GetParam());
  ExpectSequence<NodeId>(loaded.find_files("file.txt", admin), {file});
}
std::string IconCaseName(const ::testing::TestParamInfo<FileIcon>& info) {
  const char* names[] = {"Default", "Text", "Image", "Audio", "Video", "Code"};
  return names[static_cast<unsigned>(info.param)];
}
INSTANTIATE_TEST_SUITE_P(Icons, IconRoundTripTest,
                         ::testing::Values(FileIcon::Default, FileIcon::Text,
                                           FileIcon::Image, FileIcon::Audio,
                                           FileIcon::Video, FileIcon::Code),
                         IconCaseName);
TEST(VfsPersistenceTest,
     LoadsUnorderedLegacyRecordsMissingOriginalAndSparseIds) {
  auto loaded = LoadVfs(
      "VFS 1\nROOT 42\nNEXT_ID 45\nNODES 3\n"
      "43 F 44 \"a.txt\" \"/missing/a.txt\"\n44 D 42 \"assets\" \"\"\n42 D 0 "
      "\"/\" \"\"\n");
  EXPECT_EQ(loaded.root_id(), 42U);
  ExpectSequence<NodeId>(loaded.get_node(42, Admin()).children, {44});
  ExpectSequence<NodeId>(loaded.get_node(44, Admin()).children, {43});
  ExpectSequence<NodeId>(loaded.find_files("a.txt", Admin()), {43});
  EXPECT_EQ(loaded.get_node(43, Admin()).file_icon, FileIcon::Default);
  ExpectRights(loaded.get_access_rights(43, Member()), false, false);
  EXPECT_EQ(loaded.create_directory(42, "next", Admin()), 45U);
}
TEST(VfsPersistenceTest, LoadsVersionTwoWithDefaultIconsAndGrants) {
  auto loaded = LoadVfs(
      "VFS 2\nROOT 1\nNEXT_ID 3\nNODES 2\n"
      "1 D 0 \"/\" \"\"\nPOLICY 0 1\nRULE 10 1 0\n"
      "2 F 1 \"file\" \"/missing\"\nPOLICY 1 0\n");
  EXPECT_EQ(loaded.get_node(2, Admin()).file_icon, FileIcon::Default);
  ExpectRights(loaded.get_access_rights(2, Member()), true, false);
}
TEST(VfsPersistenceTest, ExhaustedIdsLoadButCannotCreateMoreNodes) {
  auto loaded =
      LoadVfs("VFS 1\nROOT 1\nNEXT_ID 0\nNODES 1\n1 D 0 \"/\" \"\"\n");
  EXPECT_THROW(loaded.create_directory(1, "new", Admin()), std::overflow_error);
  EXPECT_EQ(loaded.get_node(1, Admin()).children.size(), 0U);
}
struct SnapshotCase {
  const char* name;
  const char* text;
};
class InvalidVfsSnapshotTest : public ::testing::TestWithParam<SnapshotCase> {};
TEST_P(InvalidVfsSnapshotTest, RejectsMalformedSnapshot) {
  EXPECT_THROW(LoadVfs(GetParam().text), std::runtime_error);
}
INSTANTIATE_TEST_SUITE_P(
    CorruptVfs, InvalidVfsSnapshotTest,
    ::testing::Values(
        SnapshotCase{"Empty", ""},
        SnapshotCase{"WrongVersion",
                     "VFS 9\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" \"\"\n"},
        SnapshotCase{"ZeroRoot",
                     "VFS 1\nROOT 0\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" \"\"\n"},
        SnapshotCase{"NegativeNumber",
                     "VFS 1\nROOT 1\nNEXT_ID -1\nNODES 1\n1 D 0 \"/\" \"\"\n"},
        SnapshotCase{"OverflowNumber",
                     "VFS 1\nROOT 1\nNEXT_ID 18446744073709551616\nNODES 1\n1 "
                     "D 0 \"/\" \"\"\n"},
        SnapshotCase{"ZeroCount", "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 0\n"},
        SnapshotCase{"Truncated",
                     "VFS 1\nROOT 1\nNEXT_ID 3\nNODES 2\n1 D 0 \"/\" \"\"\n"},
        SnapshotCase{"ZeroNodeId",
                     "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 1\n0 D 0 \"/\" \"\"\n"},
        SnapshotCase{"DuplicateId",
                     "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 2\n1 D 0 \"/\" \"\"\n1 D "
                     "1 \"x\" \"\"\n"},
        SnapshotCase{"UnknownType",
                     "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 1\n1 X 0 \"/\" \"\"\n"},
        SnapshotCase{
            "RootIsFile",
            "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 1\n1 F 0 \"/\" \"/disk\"\n"},
        SnapshotCase{
            "WrongRootName",
            "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"wrong\" \"\"\n"},
        SnapshotCase{"NextIdReused",
                     "VFS 1\nROOT 1\nNEXT_ID 1\nNODES 1\n1 D 0 \"/\" \"\"\n"},
        SnapshotCase{"MissingParent",
                     "VFS 1\nROOT 1\nNEXT_ID 3\nNODES 2\n1 D 0 \"/\" \"\"\n2 D "
                     "99 \"x\" \"\"\n"},
        SnapshotCase{"Cycle",
                     "VFS 1\nROOT 1\nNEXT_ID 4\nNODES 3\n1 D 0 \"/\" \"\"\n2 D "
                     "3 \"a\" \"\"\n3 D 2 \"b\" \"\"\n"},
        SnapshotCase{"DuplicateSiblingNames",
                     "VFS 1\nROOT 1\nNEXT_ID 4\nNODES 3\n1 D 0 \"/\" \"\"\n2 D "
                     "1 \"same\" \"\"\n3 F 1 \"same\" \"/disk\"\n"},
        SnapshotCase{"FileAsParent",
                     "VFS 1\nROOT 1\nNEXT_ID 4\nNODES 3\n1 D 0 \"/\" \"\"\n2 F "
                     "1 \"f\" \"/disk\"\n3 D 2 \"child\" \"\"\n"},
        SnapshotCase{"InvalidName",
                     "VFS 1\nROOT 1\nNEXT_ID 3\nNODES 2\n1 D 0 \"/\" \"\"\n2 D "
                     "1 \"..\" \"\"\n"},
        SnapshotCase{"EmptyFilePath",
                     "VFS 1\nROOT 1\nNEXT_ID 3\nNODES 2\n1 D 0 \"/\" \"\"\n2 F "
                     "1 \"f\" \"\"\n"},
        SnapshotCase{
            "FolderHasPhysicalPath",
            "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" \"/disk\"\n"},
        SnapshotCase{"UnquotedName",
                     "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 / \"\"\n"},
        SnapshotCase{
            "TrailingData",
            "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" \"\"\njunk"},
        SnapshotCase{"RootInheritance",
                     "VFS 2\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" "
                     "\"\"\nPOLICY 1 0\n"},
        SnapshotCase{"InvalidBoolean",
                     "VFS 2\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" "
                     "\"\"\nPOLICY 2 0\n"},
        SnapshotCase{"ZeroPolicyGroup",
                     "VFS 2\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" "
                     "\"\"\nPOLICY 0 1\nRULE 0 1 0\n"},
        SnapshotCase{"DuplicatePolicyGroup",
                     "VFS 2\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" "
                     "\"\"\nPOLICY 0 2\nRULE 10 1 0\nRULE 10 0 1\n"},
        SnapshotCase{
            "UnknownIcon",
            "VFS 3\nROOT 1\nNEXT_ID 3\nNODES 2\n1 D 0 \"/\" \"\"\nPOLICY 0 "
            "0\nICON 0\n2 F 1 \"f\" \"/disk\"\nPOLICY 1 0\nICON 6\n"},
        SnapshotCase{"FolderIcon",
                     "VFS 3\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" "
                     "\"\"\nPOLICY 0 0\nICON 2\n"},
        SnapshotCase{"MissingIcon",
                     "VFS 3\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" "
                     "\"\"\nPOLICY 0 0\n"}),
    [](const auto& info) { return info.param.name; });

constexpr char kRegistry[] =
    "USER_MANAGER 1\nADMIN 1\nNEXT_USER_ID 3\nNEXT_GROUP_ID 2\nGROUPS 1\nGROUP "
    "1 \"artists\"\nUSERS 2\nUSER 1 \"admin\" 1 0\nUSER 2 \"reader\" 0 1 1\n";
struct RegistryMutation {
  const char* name;
  const char* from;
  const char* to;
};
class InvalidRegistryTest : public ::testing::TestWithParam<RegistryMutation> {
};
TEST_P(InvalidRegistryTest, RejectsInvalidFieldOrReference) {
  std::string text = kRegistry;
  const auto& p = GetParam();
  auto at = text.find(p.from);
  ASSERT_NE(at, std::string::npos);
  text.replace(at, std::char_traits<char>::length(p.from), p.to);
  EXPECT_THROW(LoadUsers(text), std::runtime_error);
}
INSTANTIATE_TEST_SUITE_P(
    CorruptRegistry, InvalidRegistryTest,
    ::testing::Values(
        RegistryMutation{"Version", "USER_MANAGER 1", "USER_MANAGER 2"},
        RegistryMutation{"ZeroAdmin", "ADMIN 1", "ADMIN 0"},
        RegistryMutation{"MissingAdmin", "ADMIN 1", "ADMIN 99"},
        RegistryMutation{"UserCounterReused", "NEXT_USER_ID 3",
                         "NEXT_USER_ID 2"},
        RegistryMutation{"GroupCounterReused", "NEXT_GROUP_ID 2",
                         "NEXT_GROUP_ID 1"},
        RegistryMutation{"NegativeCounter", "NEXT_USER_ID 3",
                         "NEXT_USER_ID -1"},
        RegistryMutation{"OverflowCounter", "NEXT_USER_ID 3",
                         "NEXT_USER_ID 18446744073709551616"},
        RegistryMutation{"ZeroGroup", "GROUP 1 \"artists\"",
                         "GROUP 0 \"artists\""},
        RegistryMutation{"DuplicateGroup", "GROUPS 1\nGROUP 1 \"artists\"",
                         "GROUPS 2\nGROUP 1 \"artists\"\nGROUP 1 \"copy\""},
        RegistryMutation{"EmptyGroupName", "\"artists\"", "\"\""},
        RegistryMutation{"NoUsers", "USERS 2", "USERS 0"},
        RegistryMutation{"ZeroUser", "USER 2 \"reader\"", "USER 0 \"reader\""},
        RegistryMutation{"DuplicateUser", "USER 2 \"reader\"",
                         "USER 1 \"reader\""},
        RegistryMutation{"UnknownMembership", "USER 2 \"reader\" 0 1 1",
                         "USER 2 \"reader\" 0 1 999"},
        RegistryMutation{"NonBooleanAdmin", "USER 1 \"admin\" 1 0",
                         "USER 1 \"admin\" 2 0"},
        RegistryMutation{"AdminWithoutPrivileges", "USER 1 \"admin\" 1 0",
                         "USER 1 \"admin\" 0 0"},
        RegistryMutation{"UnquotedName", "\"reader\"", "reader"},
        RegistryMutation{"EmptyUserName", "\"reader\"", "\"\""}),
    [](const auto& info) { return info.param.name; });
TEST(UserRegistryPersistenceTest, RoundTripRetainsNamesMembershipsAndNextIds) {
  auto users = MakeUserManager();
  auto admin = users.admin_id();
  auto group = users.create_group("artists \"x\" \\\n", admin);
  auto user = users.create_user("Саша", admin);
  users.add_user_to_group(user, group, admin);
  users.rename_user(admin, "root", admin);
  auto loaded = LoadUsers(SaveUsers(users));
  EXPECT_EQ(loaded.admin_id(), admin);
  EXPECT_EQ(loaded.get_user(admin).name, "root");
  EXPECT_EQ(loaded.get_user(user).name, "Саша");
  EXPECT_FALSE(loaded.get_user(user).is_admin);
  ExpectSequence<GroupId>(loaded.get_user(user).group_ids, {group});
  EXPECT_EQ(loaded.get_group(group).name, "artists \"x\" \\\n");
  EXPECT_EQ(loaded.create_user("next", admin), user + 1);
  EXPECT_EQ(loaded.create_group("next", admin), group + 1);
}
TEST(UserRegistryPersistenceTest, RestoresSparseIdsAndSavedCounters) {
  auto users = LoadUsers(
      "USER_MANAGER 1\nADMIN 42\nNEXT_USER_ID 100\nNEXT_GROUP_ID 90\nGROUPS "
      "1\nGROUP 7 \"g\"\nUSERS 1\nUSER 42 \"root\" 1 1 7\n");
  EXPECT_EQ(users.admin_id(), 42U);
  ExpectSequence<GroupId>(users.get_user(42).group_ids, {7});
  EXPECT_EQ(users.create_user("new", 42), 100U);
  EXPECT_EQ(users.create_group("new", 42), 90U);
}
TEST(UserRegistryPersistenceTest,
     RejectsDuplicateMembershipTruncationAndTrailingData) {
  EXPECT_THROW(LoadUsers("USER_MANAGER 1\nADMIN 1\nNEXT_USER_ID "
                         "2\nNEXT_GROUP_ID 3\nGROUPS 2\nGROUP 1 \"a\"\nGROUP 2 "
                         "\"b\"\nUSERS 1\nUSER 1 \"admin\" 1 2 1 1\n"),
               std::runtime_error);
  EXPECT_THROW(LoadUsers(std::string(kRegistry).substr(0, 70)),
               std::runtime_error);
  EXPECT_THROW(LoadUsers(std::string(kRegistry) + "junk"), std::runtime_error);
}
TEST(UserRegistryPersistenceTest, ExhaustedCountersNeverReuseIds) {
  auto users = LoadUsers(
      "USER_MANAGER 1\nADMIN 1\nNEXT_USER_ID 0\nNEXT_GROUP_ID 0\nGROUPS "
      "0\nUSERS 1\nUSER 1 \"admin\" 1 0\n");
  EXPECT_THROW(users.create_user("new", 1), std::overflow_error);
  EXPECT_THROW(users.create_group("new", 1), std::overflow_error);
}
class RejectWrites : public std::streambuf {
  std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
  int_type overflow(int_type) override { return traits_type::eof(); }
};
TEST_F(VfsTest, SaveRejectsNonadminAndFailedOutputStreams) {
  std::ostringstream output;
  EXPECT_THROW(vfs.save(output, reader), std::runtime_error);
  EXPECT_TRUE(output.str().empty());
  output.setstate(std::ios::badbit);
  EXPECT_THROW(vfs.save(output, admin), std::runtime_error);
  RejectWrites buffer;
  std::ostream broken(&buffer);
  EXPECT_THROW(vfs.save(broken, admin), std::runtime_error);
}
TEST(UserRegistryPersistenceTest,
     SaveReportsFailedOutputAndLoadRejectsUnreadableInput) {
  auto users = MakeUserManager();
  std::ostringstream output;
  output.setstate(std::ios::badbit);
  EXPECT_THROW(users.save(output, users.admin_id()), std::runtime_error);
  RejectWrites buffer;
  std::ostream broken(&buffer);
  EXPECT_THROW(users.save(broken, users.admin_id()), std::runtime_error);
  std::istringstream input(kRegistry);
  input.setstate(std::ios::badbit);
  EXPECT_THROW(UserManager::load(input, MakeUsers(), MakeGroups()),
               std::runtime_error);
}
TEST(VfsPersistenceTest,
     LoadRejectsUnreadableStreamAndInvalidDestinationStorage) {
  const std::string valid =
      "VFS 1\nROOT 1\nNEXT_ID 2\nNODES 1\n1 D 0 \"/\" \"\"\n";
  std::istringstream input(valid);
  input.setstate(std::ios::badbit);
  EXPECT_THROW(VirtualFileSystem::load(input, MakeNodes(), MakeIndex()),
               std::runtime_error);
  std::istringstream second(valid);
  EXPECT_THROW(VirtualFileSystem::load(second, nullptr, MakeIndex()),
               std::invalid_argument);
}
}  // namespace core_test
