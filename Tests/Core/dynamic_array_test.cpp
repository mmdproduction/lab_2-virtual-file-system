#include "Sequence/DynamicArray/DynamicArray.hpp"
#include "test_support.hpp"

namespace core_test {
TEST(DynamicArrayTest, EmptyArrayRejectsBothAccessOverloads) {
  DynamicArray<int> array;
  EXPECT_EQ(array.size(), 0U);
  EXPECT_THROW(array.get(0), std::out_of_range);
  const auto& constant = array;
  EXPECT_THROW(constant.get(0), std::out_of_range);
}
TEST(DynamicArrayTest, SizedArrayValueInitializesElements) {
  DynamicArray<int> array(3);
  ASSERT_EQ(array.size(), 3U);
  EXPECT_EQ(array.get(0), 0);
  EXPECT_EQ(array.get(1), 0);
  EXPECT_EQ(array.get(2), 0);
  array.get(1) = 17;
  const auto& constant = array;
  EXPECT_EQ(constant.get(1), 17);
  EXPECT_THROW(array.get(3), std::out_of_range);
  EXPECT_THROW(constant.get(3), std::out_of_range);
}
TEST(DynamicArrayTest, CopyConstructorOwnsIndependentStorage) {
  DynamicArray<std::string> source(2);
  source.get(0) = "first";
  source.get(1) = "second";
  DynamicArray<std::string> copy(source);
  copy.get(0) = "changed";
  EXPECT_EQ(source.get(0), "first");
  EXPECT_EQ(copy.get(1), "second");
}
TEST(DynamicArrayTest, AssignmentReplacesSizeAndDoesNotAlias) {
  DynamicArray<int> source(2);
  source.get(0) = 3;
  source.get(1) = 9;
  DynamicArray<int> target(5);
  EXPECT_EQ(&(target = source), &target);
  ASSERT_EQ(target.size(), 2U);
  target.get(1) = 77;
  EXPECT_EQ(source.get(1), 9);
  EXPECT_EQ(target.get(0), 3);
  const auto& alias = source;
  source = alias;
  EXPECT_EQ(source.get(0), 3);
  EXPECT_EQ(source.get(1), 9);
  target = DynamicArray<int>();
  EXPECT_EQ(target.size(), 0U);
}
TEST(DynamicArrayTest, ResizePreservesPrefixAndInitializesNewElements) {
  DynamicArray<int> array(2);
  array.get(0) = 4;
  array.get(1) = 8;
  array.resize(4);
  ASSERT_EQ(array.size(), 4U);
  EXPECT_EQ(array.get(0), 4);
  EXPECT_EQ(array.get(1), 8);
  EXPECT_EQ(array.get(2), 0);
  EXPECT_EQ(array.get(3), 0);
  array.resize(1);
  ASSERT_EQ(array.size(), 1U);
  EXPECT_EQ(array.get(0), 4);
  EXPECT_THROW(array.get(1), std::out_of_range);
  array.resize(0);
  array.resize(1);
  EXPECT_EQ(array.get(0), 0);
}
TEST(DynamicArrayTest, EqualSizeResizeDoesNotCopyElements) {
  DynamicArray<CopyValue> array(2);
  array.get(0).value = 10;
  FailCopies fail(0);
  EXPECT_NO_THROW(array.resize(2));
  EXPECT_EQ(array.get(0).value, 10);
}
TEST(DynamicArrayTest, SwapExchangesSizesAndValues) {
  DynamicArray<int> first(1);
  first.get(0) = 7;
  DynamicArray<int> second(2);
  second.get(0) = 8;
  second.get(1) = 9;
  first.swap(second);
  ASSERT_EQ(first.size(), 2U);
  ASSERT_EQ(second.size(), 1U);
  EXPECT_EQ(first.get(0), 8);
  EXPECT_EQ(first.get(1), 9);
  EXPECT_EQ(second.get(0), 7);
}
TEST(DynamicArrayTest, FailedCopyConstructorReleasesItsElements) {
  const int before = CopyValue::alive;
  {
    DynamicArray<CopyValue> source(3);
    source.get(0).value = 12;
    const int with_source = CopyValue::alive;
    {
      FailCopies fail(1);
      EXPECT_THROW((DynamicArray<CopyValue>(source)), std::runtime_error);
    }
    EXPECT_EQ(CopyValue::alive, with_source);
    EXPECT_EQ(source.get(0).value, 12);
  }
  EXPECT_EQ(CopyValue::alive, before);
}
TEST(DynamicArrayTest, FailedAssignmentAndResizePreserveDestination) {
  DynamicArray<CopyValue> source(3);
  source.get(0).value = 1;
  DynamicArray<CopyValue> target(2);
  target.get(0).value = 42;
  target.get(1).value = 43;
  const int before = CopyValue::alive;
  {
    FailCopies fail(1);
    EXPECT_THROW(target = source, std::runtime_error);
    EXPECT_EQ(target.size(), 2U);
    EXPECT_EQ(target.get(0).value, 42);
    EXPECT_EQ(CopyValue::alive, before);
  }
  {
    FailCopies fail(1);
    EXPECT_THROW(target.resize(5), std::runtime_error);
    EXPECT_EQ(target.size(), 2U);
    EXPECT_EQ(target.get(1).value, 43);
    EXPECT_EQ(CopyValue::alive, before);
  }
}
}  // namespace core_test
