#include "starkware/utils/json.h"

#include <sstream>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/utils/json_builder.h"

namespace starkware {
namespace {

using testing::ElementsAre;
using testing::HasSubstr;

/*
  Creates a simple Json object for the tests.
*/
JsonValue SampleJson() {
  JsonBuilder builder;
  auto fri = builder["stark"]["fri"];
  fri["int"] = 5;
  fri["string"] = "hello";
  fri["uint64"] = Pow2(40);
  fri["negativeInt"] = -5;
  fri["field_elements"]
      .Append(BaseFieldElement::FromUint(0x17))
      .Append(BaseFieldElement::FromUint(0x80));
  fri["int_array"].Append(10).Append(20).Append(30);
  fri["bools"].Append(true).Append(false).Append(true);
  JsonBuilder sub;
  sub["array"].Append(5).Append(8).Append("str").Append("another str");
  JsonValue json_sub = sub.Build();
  fri["mixed_array"].Append(0).Append("string").Append(json_sub);
  fri["int_as_string"] = std::to_string(Pow2(40));
  return builder.Build();
}

class JsonTest : public testing::Test {
 public:
  JsonValue root = SampleJson();
};

TEST_F(JsonTest, ObjectAccess) {
  EXPECT_ASSERT(
      root["stark"]["fri2"]["int"], HasSubstr("Missing configuration object: /stark/fri2/"));
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"]["test"],
      HasSubstr("Configuration at /stark/fri/int/ is expected to be an object."));
}

TEST_F(JsonTest, ArrayAccess) {
  EXPECT_EQ(20U, root["stark"]["fri"]["int_array"][1].AsSizeT());
  EXPECT_ASSERT(root[1], HasSubstr("Configuration at / is expected to be an array."));
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"][1],
      HasSubstr("Configuration at /stark/fri/int/ is expected to be an array."));
}

TEST_F(JsonTest, HasValue) {
  EXPECT_TRUE(root["stark"].HasValue());
  EXPECT_FALSE(root["stark2"].HasValue());
}

TEST_F(JsonTest, AsUint64) {
  EXPECT_EQ(Pow2(40), root["stark"]["fri"]["uint64"].AsUint64());
  EXPECT_EQ(5, root["stark"]["fri"]["int"].AsUint64());
  EXPECT_ASSERT(
      root["stark"]["fri"]["string"].AsUint64(),
      HasSubstr("Configuration at /stark/fri/string/ is expected to be a uint64."));
  EXPECT_ASSERT(
      root["stark"]["fri"]["negativeInt"].AsUint64(),
      HasSubstr("Configuration at /stark/fri/negativeInt/ is expected to be a uint64."));
}

TEST_F(JsonTest, AsSizeT) {
  EXPECT_EQ(5U, root["stark"]["fri"]["int"].AsSizeT());
  EXPECT_ASSERT(
      root["stark"]["fri"]["missing"].AsSizeT(),
      HasSubstr("Missing configuration value: /stark/fri/missing/"));
  EXPECT_ASSERT(
      root["stark"]["fri"]["string"].AsSizeT(),
      HasSubstr("Configuration at /stark/fri/string/ is expected to be an integer."));
}

TEST_F(JsonTest, ArrayLength) {
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"].ArrayLength(),
      HasSubstr("Configuration at /stark/fri/int/ is expected to be an array."));
  EXPECT_EQ(3U, root["stark"]["fri"]["int_array"].ArrayLength());
  EXPECT_EQ(3U, root["stark"]["fri"]["mixed_array"].ArrayLength());
  EXPECT_EQ(4U, root["stark"]["fri"]["mixed_array"][2]["array"].ArrayLength());
}

TEST_F(JsonTest, AsSizeTVector) {
  EXPECT_THAT(root["stark"]["fri"]["int_array"].AsSizeTVector(), ElementsAre(10, 20, 30));
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"].AsSizeTVector(),
      HasSubstr("Configuration at /stark/fri/int/ is expected to be an array."));
  EXPECT_ASSERT(
      root["stark"]["fri"]["mixed_array"].AsSizeTVector(),
      HasSubstr("Configuration at /stark/fri/mixed_array/1/ is expected to be an integer."));
}

TEST_F(JsonTest, AsString) {
  EXPECT_EQ("hello", root["stark"]["fri"]["string"].AsString());
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"].AsString(),
      HasSubstr("Configuration at /stark/fri/int/ is expected to be a string."));
}

TEST_F(JsonTest, AsFieldElement) {
  EXPECT_EQ(
      BaseFieldElement::FromUint(0x17),
      root["stark"]["fri"]["field_elements"][0].AsFieldElement<BaseFieldElement>());
  EXPECT_ASSERT(
      root["stark"]["fri"]["string"].AsFieldElement<BaseFieldElement>(),
      HasSubstr("does not start with '0x'"));
}

}  // namespace
}  // namespace starkware
