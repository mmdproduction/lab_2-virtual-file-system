#include "test_support.hpp"

namespace core_test {
TEST(VfsConstructionTest, RejectsNullOrPrepopulatedStorage) {
  EXPECT_THROW(VirtualFileSystem(nullptr, MakeIndex()), std::invalid_argument);
  EXPECT_THROW(VirtualFileSystem(MakeNodes(), nullptr), std::invalid_argument);
  auto storage = MakeNodes();
  storage->add(9, FSNode{});
  EXPECT_THROW(VirtualFileSystem(std::move(storage), MakeIndex()),
               std::invalid_argument);
}
TEST_F(VfsTest, RootIsEmptyFolderWithNoParentAndNoInheritance) {
  const auto& node = vfs.get_node(root, admin);
  EXPECT_NE(root, kInvalidNodeId);
  EXPECT_EQ(node.id, root);
  EXPECT_EQ(node.name, "/");
  EXPECT_EQ(node.type, FSType::Folder);
  EXPECT_EQ(node.parent_id, kInvalidNodeId);
  EXPECT_TRUE(node.physical_path.empty());
  EXPECT_EQ(node.children.size(), 0U);
  EXPECT_FALSE(node.access_policy.inherit_from_parent);
}
TEST_F(VfsTest, DirectoryCreationLinksBothParentAndChild) {
  auto first = vfs.create_directory(root, "assets", admin);
  auto child = vfs.create_directory(first, "scenes", admin);
  ExpectSequence<NodeId>(vfs.get_node(root, admin).children, {first});
  ExpectSequence<NodeId>(vfs.get_node(first, admin).children, {child});
  EXPECT_EQ(vfs.get_node(child, admin).parent_id, first);
  EXPECT_EQ(vfs.get_node(child, admin).type, FSType::Folder);
  EXPECT_TRUE(vfs.get_node(child, admin).access_policy.inherit_from_parent);
}
TEST_F(VfsTest, DuplicateNameAcrossTypesIsRejectedWithoutConsumingId) {
  auto folder = vfs.create_directory(root, "same", admin);
  EXPECT_THROW(vfs.create_directory(root, "same", admin),
               std::invalid_argument);
  EXPECT_THROW(vfs.attach_file(root, "same", original, admin),
               std::invalid_argument);
  EXPECT_EQ(vfs.create_directory(root, "next", admin), folder + 1);
}
class InvalidNameTest : public VfsTest,
                        public ::testing::WithParamInterface<const char*> {};
TEST_P(InvalidNameTest, CreateAttachAndRenameRejectInvalidNameWithoutChanges) {
  auto file = File(root);
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.create_directory(root, GetParam(), admin),
               std::invalid_argument);
  EXPECT_THROW(vfs.attach_file(root, GetParam(), original, admin),
               std::invalid_argument);
  EXPECT_THROW(vfs.rename(file, GetParam(), admin), std::invalid_argument);
  EXPECT_EQ(SaveVfs(vfs), before);
}
INSTANTIATE_TEST_SUITE_P(InvalidNames, InvalidNameTest,
                         ::testing::Values("", ".", "..", "a/b"));
TEST_F(VfsTest, AttachStoresCanonicalPathAndIndexesVirtualName) {
  auto file =
      vfs.attach_file(root, "виртуальное имя.txt",
                      (temp.path() / "." / "physical.txt").string(), admin);
  const auto& node = vfs.get_node(file, admin);
  EXPECT_EQ(node.type, FSType::File);
  EXPECT_EQ(node.name, "виртуальное имя.txt");
  EXPECT_EQ(node.parent_id, root);
  EXPECT_EQ(node.physical_path, std::filesystem::canonical(original).string());
  ExpectSequence<NodeId>(vfs.find_files("виртуальное имя.txt", admin), {file});
  EXPECT_EQ(vfs.find_files("physical.txt", admin).size(), 0U);
  EXPECT_EQ(ReadFile(original), "original contents");
}
TEST_F(VfsTest, AttachRejectsEmptyMissingOrDirectoryPathsWithoutCreatingNodes) {
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.attach_file(root, "empty", "", admin),
               std::invalid_argument);
  EXPECT_THROW(vfs.attach_file(root, "missing",
                               (temp.path() / "missing").string(), admin),
               std::invalid_argument);
  EXPECT_THROW(vfs.attach_file(root, "directory", temp.path().string(), admin),
               std::invalid_argument);
  EXPECT_EQ(SaveVfs(vfs), before);
}
TEST_F(VfsTest, FilesCannotBeParentsAndUnknownIdsAreRejected) {
  auto file = File(root);
  EXPECT_THROW(vfs.create_directory(file, "child", admin),
               std::invalid_argument);
  EXPECT_THROW(vfs.attach_file(file, "child", original, admin),
               std::invalid_argument);
  EXPECT_THROW(vfs.create_directory(999, "child", admin), std::out_of_range);
  EXPECT_THROW(vfs.get_node(999, admin), std::out_of_range);
}
TEST_F(VfsTest, SameNameFilesInDifferentDirectoriesAreDistinctSearchResults) {
  auto a = vfs.create_directory(root, "a", admin);
  auto b = vfs.create_directory(root, "b", admin);
  auto first = File(a, "same.txt");
  auto second = File(b, "same.txt");
  ExpectSequence<NodeId>(vfs.find_files("same.txt", admin), {first, second});
  EXPECT_EQ(vfs.find_files("same", admin).size(), 0U);
  EXPECT_EQ(vfs.find_files("Same.txt", admin).size(), 0U);
}
TEST_F(VfsTest, FileRenameUpdatesIndexWithoutChangingIdPathOrIcon) {
  auto file = File(root, "old");
  vfs.set_file_icon(file, FileIcon::Code, admin);
  vfs.rename(file, "new", admin);
  vfs.rename(file, "new", admin);
  EXPECT_EQ(vfs.find_files("old", admin).size(), 0U);
  ExpectSequence<NodeId>(vfs.find_files("new", admin), {file});
  const auto& node = vfs.get_node(file, admin);
  EXPECT_EQ(node.physical_path, original);
  EXPECT_EQ(node.file_icon, FileIcon::Code);
  EXPECT_EQ(ReadFile(original), "original contents");
}
TEST_F(VfsTest, RenameCollisionAndRootRenameLeaveSnapshotUntouched) {
  auto first = File(root, "first");
  vfs.create_directory(root, "second", admin);
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.rename(first, "second", admin), std::invalid_argument);
  EXPECT_THROW(vfs.rename(root, "renamed", admin), std::invalid_argument);
  EXPECT_EQ(SaveVfs(vfs), before);
}
TEST_F(VfsTest, MovingSubtreePreservesDescendantsIdsAndSearch) {
  auto a = vfs.create_directory(root, "a", admin);
  auto b = vfs.create_directory(root, "b", admin);
  auto child = vfs.create_directory(a, "child", admin);
  auto file = File(child);
  vfs.move(child, b, admin);
  EXPECT_EQ(vfs.get_node(a, admin).children.size(), 0U);
  ExpectSequence<NodeId>(vfs.get_node(b, admin).children, {child});
  EXPECT_EQ(vfs.get_node(child, admin).parent_id, b);
  EXPECT_EQ(vfs.get_node(file, admin).parent_id, child);
  ExpectSequence<NodeId>(vfs.find_files("file.txt", admin), {file});
  EXPECT_EQ(vfs.get_node(file, admin).physical_path, original);
}
TEST_F(VfsTest, MovingToSameParentIsNoOpButCyclesAndRootAreRejected) {
  auto a = vfs.create_directory(root, "a", admin);
  auto child = vfs.create_directory(a, "child", admin);
  const auto before = SaveVfs(vfs);
  vfs.move(a, root, admin);
  EXPECT_EQ(SaveVfs(vfs), before);
  EXPECT_THROW(vfs.move(a, a, admin), std::invalid_argument);
  EXPECT_THROW(vfs.move(a, child, admin), std::invalid_argument);
  EXPECT_THROW(vfs.move(root, child, admin), std::invalid_argument);
  EXPECT_EQ(SaveVfs(vfs), before);
}
TEST_F(VfsTest, MoveConflictAndFileDestinationLeaveBothParentsUnchanged) {
  auto a = vfs.create_directory(root, "a", admin);
  auto b = vfs.create_directory(root, "b", admin);
  auto first = File(a, "same");
  File(b, "same");
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.move(first, b, admin), std::invalid_argument);
  EXPECT_THROW(vfs.move(a, first, admin), std::invalid_argument);
  EXPECT_EQ(SaveVfs(vfs), before);
}
TEST_F(VfsTest, RemovingFileUpdatesParentAndIndexButKeepsOriginal) {
  auto file = File(root);
  vfs.remove(file, admin);
  EXPECT_EQ(vfs.get_node(root, admin).children.size(), 0U);
  EXPECT_EQ(vfs.find_files("file.txt", admin).size(), 0U);
  EXPECT_THROW(vfs.get_node(file, admin), std::out_of_range);
  EXPECT_EQ(ReadFile(original), "original contents");
  EXPECT_GT(vfs.create_directory(root, "new", admin), file);
}
TEST_F(VfsTest, NonrecursiveRemoveRejectsNonemptyFolderAndRoot) {
  auto folder = vfs.create_directory(root, "folder", admin);
  File(folder);
  const auto before = SaveVfs(vfs);
  EXPECT_THROW(vfs.remove(folder, admin), std::invalid_argument);
  EXPECT_THROW(vfs.remove(root, admin), std::invalid_argument);
  EXPECT_THROW(vfs.remove_recursive(root, admin), std::invalid_argument);
  EXPECT_EQ(SaveVfs(vfs), before);
}
TEST_F(VfsTest, RecursiveRemoveDeletesOnlyRequestedVirtualSubtree) {
  auto folder = vfs.create_directory(root, "folder", admin);
  auto child = vfs.create_directory(folder, "child", admin);
  auto removed = File(child, "same");
  auto survivor = File(root, "same");
  vfs.remove_recursive(folder, admin);
  for (NodeId id : {folder, child, removed})
    EXPECT_THROW(vfs.get_node(id, admin), std::out_of_range);
  ExpectSequence<NodeId>(vfs.get_node(root, admin).children, {survivor});
  ExpectSequence<NodeId>(vfs.find_files("same", admin), {survivor});
  EXPECT_EQ(ReadFile(original), "original contents");
}
TEST_F(VfsTest, EmptyFolderCanBeRemovedAndMissingNodeIsRejected) {
  auto folder = vfs.create_directory(root, "empty", admin);
  vfs.remove(folder, admin);
  EXPECT_EQ(vfs.get_node(root, admin).children.size(), 0U);
  EXPECT_THROW(vfs.remove(folder, admin), std::out_of_range);
  EXPECT_THROW(vfs.move(folder, root, admin), std::out_of_range);
}
TEST_F(VfsTest, IconChangesAreFileOnlyAndInvalidValuesPreserveOldIcon) {
  auto file = File(root);
  vfs.set_file_icon(file, FileIcon::Audio, admin);
  EXPECT_THROW(vfs.set_file_icon(root, FileIcon::Code, admin),
               std::invalid_argument);
  EXPECT_THROW(vfs.set_file_icon(file, static_cast<FileIcon>(255), admin),
               std::invalid_argument);
  EXPECT_EQ(vfs.get_node(file, admin).file_icon, FileIcon::Audio);
}
}  // namespace core_test
