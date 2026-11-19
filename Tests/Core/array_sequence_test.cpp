#include "test_support.hpp"

namespace core_test {
TEST(ArraySequenceTest, WorksThroughSequenceInterface) {
  std::unique_ptr<Sequence<int>> sequence =
      std::make_unique<ArraySequence<int>>();
  sequence->append(10);
  sequence->append(20);
  sequence->get(0) = 11;
  const Sequence<int>& constant = *sequence;
  ASSERT_EQ(constant.size(), 2U);
  EXPECT_EQ(constant.get(0), 11);
  EXPECT_EQ(constant.get(1), 20);
  sequence->remove_at(0);
  EXPECT_EQ(sequence->get(0), 20);
  sequence->clear();
  EXPECT_EQ(sequence->size(), 0U);
}
TEST(ArraySequenceTest, MultipleGrowthsPreserveAllValues) {
  ArraySequence<int> sequence;
  for (int i = 0; i < 257; ++i) sequence.append(i * 3);
  ASSERT_EQ(sequence.size(), 257U);
  for (int i = 0; i < 257; ++i) EXPECT_EQ(sequence.get(i), i * 3) << i;
}
TEST(ArraySequenceTest, AppendMayReferToExistingElementDuringReallocation) {
  ArraySequence<std::string> sequence;
  sequence.append("first");
  sequence.append("second");
  sequence.append(sequence.get(0));
  ExpectSequence(sequence, {std::string("first"), std::string("second"),
                            std::string("first")});
}
TEST(ArraySequenceTest, BoundsUseLogicalLengthRatherThanSpareCapacity) {
  ArraySequence<int> sequence;
  sequence.append(1);
  sequence.append(2);
  sequence.append(3);
  EXPECT_THROW(sequence.get(3), std::out_of_range);
  const auto& constant = sequence;
  EXPECT_THROW(constant.get(3), std::out_of_range);
  EXPECT_THROW(sequence.remove_at(3), std::out_of_range);
  ExpectSequence(sequence, {1, 2, 3});
}
struct RemoveCase {
  std::size_t index;
  int expected[3];
  const char* name;
};
class SequenceRemovalTest : public ::testing::TestWithParam<RemoveCase> {};
TEST_P(SequenceRemovalTest, PreservesRemainingOrder) {
  ArraySequence<int> sequence;
  for (int value : {10, 20, 30, 40}) sequence.append(value);
  const auto& p = GetParam();
  sequence.remove_at(p.index);
  ASSERT_EQ(sequence.size(), 3U);
  for (std::size_t i = 0; i < 3; ++i) EXPECT_EQ(sequence.get(i), p.expected[i]);
}
INSTANTIATE_TEST_SUITE_P(Positions, SequenceRemovalTest,
                         ::testing::Values(RemoveCase{0, {20, 30, 40}, "First"},
                                           RemoveCase{
                                               1, {10, 30, 40}, "Middle"},
                                           RemoveCase{3, {10, 20, 30}, "Last"}),
                         [](const auto& info) { return info.param.name; });
TEST(ArraySequenceTest, CopyAndAssignmentAreIndependent) {
  ArraySequence<int> source;
  source.append(10);
  source.append(20);
  ArraySequence<int> copy(source);
  copy.get(0) = 90;
  ArraySequence<int> assigned;
  assigned.append(1);
  assigned = source;
  assigned.remove_at(0);
  ExpectSequence(source, {10, 20});
  ExpectSequence(copy, {90, 20});
  ExpectSequence(assigned, {20});
  const auto& alias = source;
  source = alias;
  ExpectSequence(source, {10, 20});
}
TEST(ArraySequenceTest, ClearAndRemoveLastAllowReuse) {
  ArraySequence<int> sequence;
  EXPECT_THROW(sequence.remove_at(0), std::out_of_range);
  sequence.append(8);
  sequence.remove_at(0);
  EXPECT_EQ(sequence.size(), 0U);
  sequence.append(9);
  sequence.clear();
  sequence.clear();
  EXPECT_THROW(sequence.get(0), std::out_of_range);
  sequence.append(10);
  ExpectSequence(sequence, {10});
}
TEST(ArraySequenceTest, SwapExchangesLogicalLengthsAndStorage) {
  ArraySequence<int> first;
  first.append(1);
  ArraySequence<int> second;
  second.append(2);
  second.append(3);
  first.swap(second);
  ExpectSequence(first, {2, 3});
  ExpectSequence(second, {1});
}
TEST(ArraySequenceTest, FailedAppendAndRemoveLeaveContentsAndNoLeaks) {
  ArraySequence<CopyValue> sequence;
  CopyValue first;
  first.value = 10;
  CopyValue second;
  second.value = 20;
  sequence.append(first);
  sequence.append(second);
  const int before = CopyValue::alive;
  {
    FailCopies fail(0);
    EXPECT_THROW(sequence.append(first), std::runtime_error);
    EXPECT_EQ(sequence.size(), 2U);
    EXPECT_EQ(sequence.get(0).value, 10);
    EXPECT_EQ(CopyValue::alive, before);
  }
  {
    FailCopies fail(0);
    EXPECT_THROW(sequence.remove_at(0), std::runtime_error);
    EXPECT_EQ(sequence.size(), 2U);
    EXPECT_EQ(sequence.get(1).value, 20);
    EXPECT_EQ(CopyValue::alive, before);
  }
}
}  // namespace core_test
