#include "test_support.hpp"

namespace core_test {
class FileNameIndexTest : public ::testing::Test {
 protected:
  FileNameIndex index{MakeIndex()};
};
TEST(FileNameIndexConstructionTest, RejectsNullAndPrepopulatedStorage) {
  EXPECT_THROW(FileNameIndex(nullptr), std::invalid_argument);
  auto storage = MakeIndex();
  ArraySequence<NodeId> ids;
  ids.append(1);
  storage->add("old", ids);
  EXPECT_THROW(FileNameIndex(std::move(storage)), std::invalid_argument);
}
TEST_F(FileNameIndexTest, SupportsMultipleIdsForOneName) {
  index.add("same.txt", 10);
  index.add("same.txt", 20);
  index.add("other.txt", 30);
  ExpectSequence<NodeId>(index.find("same.txt"), {10, 20});
  ExpectSequence<NodeId>(index.find("other.txt"), {30});
  EXPECT_EQ(index.find("absent").size(), 0U);
}
TEST_F(FileNameIndexTest, DuplicateAndInvalidAddsPreserveExistingEntries) {
  index.add("a", 10);
  EXPECT_THROW(index.add("a", 10), std::invalid_argument);
  EXPECT_THROW(index.add("", 20), std::invalid_argument);
  EXPECT_THROW(index.add("a", kInvalidNodeId), std::invalid_argument);
  ExpectSequence<NodeId>(index.find("a"), {10});
}
TEST_F(FileNameIndexTest, FindReturnsIndependentSequence) {
  index.add("a", 10);
  auto result = index.find("a");
  result.get(0) = 99;
  result.append(100);
  ExpectSequence<NodeId>(index.find("a"), {10});
}
TEST_F(FileNameIndexTest, RemovalRetainsOtherIdsAndAllowsReaddingLastBucket) {
  index.add("a", 10);
  index.add("a", 20);
  index.add("a", 30);
  index.remove("a", 20);
  ExpectSequence<NodeId>(index.find("a"), {10, 30});
  index.remove("a", 10);
  index.remove("a", 30);
  EXPECT_EQ(index.find("a").size(), 0U);
  index.add("a", 40);
  ExpectSequence<NodeId>(index.find("a"), {40});
}
TEST_F(FileNameIndexTest, MissingRemovalPreservesBucket) {
  index.add("a", 10);
  EXPECT_THROW(index.remove("a", 20), std::out_of_range);
  EXPECT_THROW(index.remove("missing", 10), std::out_of_range);
  ExpectSequence<NodeId>(index.find("a"), {10});
}
TEST_F(FileNameIndexTest, RenameMovesOnlySpecifiedIdAndMergesExistingBucket) {
  index.add("old", 10);
  index.add("old", 20);
  index.add("new", 30);
  index.rename("old", "new", 10);
  ExpectSequence<NodeId>(index.find("old"), {20});
  ExpectSequence<NodeId>(index.find("new"), {30, 10});
  index.rename("old", "last", 20);
  EXPECT_EQ(index.find("old").size(), 0U);
  ExpectSequence<NodeId>(index.find("last"), {20});
}
TEST_F(FileNameIndexTest,
       SameNameRenameRequiresExistingIdAndOtherwiseDoesNothing) {
  index.add("a", 10);
  index.rename("a", "a", 10);
  EXPECT_THROW(index.rename("a", "a", 20), std::out_of_range);
  ExpectSequence<NodeId>(index.find("a"), {10});
}
TEST_F(FileNameIndexTest, RenameConflictPreservesBothBuckets) {
  index.add("old", 10);
  index.add("new", 10);
  EXPECT_THROW(index.rename("old", "new", 10), std::invalid_argument);
  ExpectSequence<NodeId>(index.find("old"), {10});
  ExpectSequence<NodeId>(index.find("new"), {10});
}
TEST_F(FileNameIndexTest, InvalidRenamePreservesSource) {
  index.add("old", 10);
  EXPECT_THROW(index.rename("old", "", 10), std::invalid_argument);
  EXPECT_THROW(index.rename("", "new", 10), std::invalid_argument);
  EXPECT_THROW(index.rename("old", "new", 0), std::invalid_argument);
  EXPECT_THROW(index.rename("absent", "new", 10), std::out_of_range);
  ExpectSequence<NodeId>(index.find("old"), {10});
  EXPECT_EQ(index.find("new").size(), 0U);
}
}  // namespace core_test
