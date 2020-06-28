#include "starkware/stl_utils/containers.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::UnorderedElementsAre;

TEST(StlContainers, GetMapKeys) {
  std::map<int, int> m;
  m[0] = 5;
  m[6] = 9;
  m[3] = 7;
  EXPECT_THAT(Keys(m), UnorderedElementsAre(0, 6, 3));
}

TEST(StlContainers, Count) {
  std::vector<int> v = {1, -1, 2, 3, 2, 1, 1};
  EXPECT_EQ(Count(v, 1), 3U);
  EXPECT_EQ(Count(v, -1), 1U);
  EXPECT_EQ(Count(v, 2), 2U);
  EXPECT_EQ(Count(v, 3), 1U);
}

TEST(StlContainers, Sum) {
  EXPECT_EQ(Sum(std::vector<int>{1, -1, 2, 3, 2, 1, 1}), 9U);
  EXPECT_EQ(Sum(std::vector<int>{}), 0U);
  EXPECT_EQ(Sum(std::vector<int>{5}), 5U);

  EXPECT_EQ(Sum(std::vector<int>{5}, 0), 5U);
  EXPECT_EQ(Sum(std::vector<int>{5}, 3), 8U);
  EXPECT_EQ(Sum(std::vector<int>{}, 0), 0U);
  EXPECT_EQ(Sum(std::vector<int>{}, 3), 3U);
}

TEST(StlContainers, AreDisjoint) {
  EXPECT_FALSE(AreDisjoint(std::set<int>{1}, std::set<int>{1}));
  EXPECT_TRUE(AreDisjoint(std::set<unsigned>{1}, std::set<unsigned>{7}));
  EXPECT_FALSE(AreDisjoint(std::set<float>{1}, std::set<float>{2, 1}));
  EXPECT_TRUE(AreDisjoint(std::set<int>{1, 2}, std::set<int>{7, 9}));
  EXPECT_FALSE(AreDisjoint(std::set<int>{19, 17, 0}, std::set<int>{2, 1, 11, 23, 19}));
  EXPECT_TRUE(AreDisjoint(std::set<int>{8, 7, 6, 5}, std::set<int>{3}));
}

TEST(StlContainers, HasDuplicates) {
  EXPECT_FALSE(HasDuplicates<int>(std::vector<int>{}));
  EXPECT_FALSE(HasDuplicates<int>(std::vector<int>{1, 10, 5, 3}));
  EXPECT_TRUE(HasDuplicates<int>(std::vector<int>{1, 10, 5, 3, 10, 7}));
  EXPECT_TRUE(HasDuplicates<int>(std::vector<int>{1, 1, 2, 2, 3, 3}));
  EXPECT_TRUE(HasDuplicates<int>(std::vector<int>{1, 10, 5, 3, 7, 1}));
}

/*
  Tests the "<<" operator for an STL set. Test cases are: empty set, singleton, and a 4-element set
  constructed from scratch.
*/
TEST(StlContainer, SetToStream) {
  std::set<unsigned> some_set;
  // Empty set.
  std::stringstream some_stream;
  some_stream << some_set;
  EXPECT_EQ("{}", some_stream.str());

  // Singleton.
  some_stream.clear();
  some_stream.str(std::string());
  some_set.insert(314);
  some_stream << some_set;
  EXPECT_EQ("{314}", some_stream.str());

  // 4-element set from scratch.
  some_set = {0, 1, 2, 3};
  some_stream.clear();
  some_stream.str(std::string());
  some_stream << some_set;
  EXPECT_EQ("{0, 1, 2, 3}", some_stream.str());
}

/*
  Tests the "<<" operator for an STL vector. Test cases are: empty set, singleton, and a 4-element
  set constructed from scratch.
*/
TEST(StlContainer, VectorToStream) {
  std::vector<unsigned> v;
  // Empty set.
  std::stringstream s;
  s << v;
  EXPECT_EQ("[]", s.str());

  // Singleton.
  s.clear();
  s.str(std::string());
  v.push_back(314);
  s << v;
  EXPECT_EQ("[314]", s.str());

  // 4-element set from scratch.
  v = {0, 1, 2, 3};
  s.clear();
  s.str(std::string());
  s << v;
  EXPECT_EQ("[0, 1, 2, 3]", s.str());
}

}  // namespace
