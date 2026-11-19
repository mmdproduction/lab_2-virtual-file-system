#include <random>

#include "test_support.hpp"

namespace core_test {
TEST(AvlDictionaryTest, InterfaceSupportsMutableAndConstLookup) {
  std::unique_ptr<IDictionary<int, std::string>> dictionary =
      std::make_unique<AvlDictionary<int, std::string>>();
  dictionary->add(20, "twenty");
  dictionary->add(10, "ten");
  dictionary->get(20) = "updated";
  const auto& constant = *dictionary;
  EXPECT_EQ(constant.get(20), "updated");
  EXPECT_EQ(dictionary->size(), 2U);
  EXPECT_TRUE(dictionary->contains_key(10));
  EXPECT_FALSE(dictionary->contains_key(99));
}
TEST(AvlDictionaryTest, DuplicateAndMissingKeysDoNotChangeDictionary) {
  AvlDictionary<int, std::string> dictionary;
  dictionary.add(7, "original");
  EXPECT_THROW(dictionary.add(7, "replaced"), std::invalid_argument);
  EXPECT_THROW(dictionary.get(8), std::out_of_range);
  const auto& constant = dictionary;
  EXPECT_THROW(constant.get(8), std::out_of_range);
  EXPECT_THROW(dictionary.remove(8), std::out_of_range);
  EXPECT_EQ(dictionary.size(), 1U);
  EXPECT_EQ(dictionary.get(7), "original");
}
TEST(AvlDictionaryTest, ErasingNodeWithTwoChildrenPreservesOtherEntries) {
  AvlDictionary<int, int> dictionary;
  for (int key : {20, 10, 30, 25, 40, 22}) dictionary.add(key, key + 100);
  dictionary.remove(20);
  EXPECT_FALSE(dictionary.contains_key(20));
  EXPECT_EQ(dictionary.size(), 5U);
  for (int key : {10, 22, 25, 30, 40})
    EXPECT_EQ(dictionary.get(key), key + 100);
}
TEST(AvlDictionaryTest, TraversalIsSortedAndVisitorFailureDoesNotDamageTree) {
  AvlDictionary<int, std::string> dictionary;
  dictionary.add(3, "c");
  dictionary.add(1, "a");
  dictionary.add(2, "b");
  ArraySequence<int> keys;
  dictionary.for_each([&](int key, const std::string& value) {
    keys.append(key);
    EXPECT_EQ(value.size(), 1U);
  });
  ExpectSequence(keys, {1, 2, 3});
  EXPECT_THROW(dictionary.for_each([](int, const std::string&) {
    throw std::runtime_error("visitor");
  }),
               std::runtime_error);
  EXPECT_EQ(dictionary.size(), 3U);
  EXPECT_EQ(dictionary.get(2), "b");
}
struct DirectionCompare {
  bool reverse = false;
  bool operator()(int first, int second) const {
    return reverse ? first > second : first < second;
  }
};
TEST(AvlDictionaryTest, ConstructorHonorsStatefulComparator) {
  AvlDictionary<int, int, DirectionCompare> dictionary(DirectionCompare{true});
  for (int key : {1, 3, 2}) dictionary.add(key, key * 10);
  ArraySequence<int> order;
  dictionary.for_each([&](int key, int) { order.append(key); });
  ExpectSequence(order, {3, 2, 1});
  EXPECT_EQ(dictionary.get(2), 20);
}
TEST(AvlDictionaryTest, CopiesOwnIndependentNodesAndValues) {
  AvlDictionary<int, ArraySequence<int>> source;
  ArraySequence<int> values;
  values.append(10);
  source.add(1, values);
  AvlDictionary<int, ArraySequence<int>> copy(source);
  copy.get(1).get(0) = 20;
  AvlDictionary<int, ArraySequence<int>> assigned;
  assigned = source;
  assigned.remove(1);
  ExpectSequence(source.get(1), {10});
  ExpectSequence(copy.get(1), {20});
  EXPECT_EQ(assigned.size(), 0U);
}
TEST(AvlDictionaryTest, ClearAndEmptyTraversalAllowReuse) {
  AvlDictionary<int, int> dictionary;
  int visits = 0;
  dictionary.for_each([&](int, int) { ++visits; });
  EXPECT_EQ(visits, 0);
  dictionary.add(1, 2);
  dictionary.clear();
  dictionary.clear();
  EXPECT_EQ(dictionary.size(), 0U);
  EXPECT_FALSE(dictionary.contains_key(1));
  dictionary.add(3, 4);
  EXPECT_EQ(dictionary.get(3), 4);
}
TEST(AvlDictionaryTest, MonotoneInsertAndEraseKeepEveryRemainingValue) {
  AvlDictionary<int, int> dictionary;
  for (int key = 0; key < 512; ++key) dictionary.add(key, key * 7);
  for (int key = 0; key < 512; key += 2) dictionary.remove(key);
  ASSERT_EQ(dictionary.size(), 256U);
  for (int key = 0; key < 512; ++key) {
    EXPECT_EQ(dictionary.contains_key(key), key % 2 == 1);
    if (key % 2) {
      EXPECT_EQ(dictionary.get(key), key * 7);
    }
  }
}
TEST(AvlDictionaryTest,
     DeterministicMixedOperationsMatchIndependentArrayOracle) {
  AvlDictionary<int, int> dictionary;
  bool present[64]{};
  int expected[64]{};
  std::mt19937 random(1701);
  for (int step = 0; step < 400; ++step) {
    SCOPED_TRACE(step);
    const int key = static_cast<int>(random() % 64);
    if (random() % 2 == 0) {
      if (present[key]) {
        EXPECT_THROW(dictionary.add(key, step), std::invalid_argument);
      } else {
        dictionary.add(key, step);
        present[key] = true;
        expected[key] = step;
      }
    } else {
      if (present[key]) {
        dictionary.remove(key);
        present[key] = false;
      } else {
        EXPECT_THROW(dictionary.remove(key), std::out_of_range);
      }
    }
    std::size_t count = 0;
    for (int probe = 0; probe < 64; ++probe) {
      ASSERT_EQ(dictionary.contains_key(probe), present[probe]) << probe;
      if (present[probe]) {
        ++count;
        ASSERT_EQ(dictionary.get(probe), expected[probe]) << probe;
      }
    }
    ASSERT_EQ(dictionary.size(), count);
  }
}
}  // namespace core_test
