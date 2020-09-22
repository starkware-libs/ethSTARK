#include "starkware/algebra/field_operations.h"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

template <typename FieldElementT>
class FieldTest : public ::testing::Test {
 public:
  Prng prng;
  FieldElementT RandomElement() { return FieldElementT::RandomElement(&prng); }
};

using TestedFieldTypes = ::testing::Types<BaseFieldElement, ExtensionFieldElement>;
TYPED_TEST_CASE(FieldTest, TestedFieldTypes);

// --- Test field basic operations ---

TYPED_TEST(FieldTest, EqOperator) {
  const auto r = this->RandomElement();
  ASSERT_TRUE(r == r);
}

TYPED_TEST(FieldTest, NotEqOperator) {
  const auto r1 = this->RandomElement();
  const auto r1_plus_r2 = r1 + RandomNonZeroElement<TypeParam>(&this->prng);
  ASSERT_TRUE(r1 != r1_plus_r2);
  ASSERT_TRUE(r1_plus_r2 != r1);
}

TYPED_TEST(FieldTest, Assignment) {
  const auto r = this->RandomElement();
  const auto x = r;  // NOLINT
  ASSERT_TRUE(r == x);
}

/*
  Checks for b != 0, that a * b^-1 = a / b.
*/
TYPED_TEST(FieldTest, DivisionIsMultiplicationByInverse) {
  const auto a = this->RandomElement();
  const auto b = RandomNonZeroElement<TypeParam>(&this->prng);
  const auto b_inverse = b.Inverse();
  ASSERT_EQ(a * b_inverse, a / b);
}

/*
  Checks that 0 - a = (-a).
*/
TYPED_TEST(FieldTest, UnaryMinusIsLikeZeroMinusElement) {
  const auto a = this->RandomElement();
  ASSERT_EQ(TypeParam::Zero() - a, -a);
}

/*
  Checks that (-a) + (b) = b - a.
*/
TYPED_TEST(FieldTest, UnaryMinusIsLikeMinusOperation) {
  const auto a = this->RandomElement();
  const auto b = this->RandomElement();
  ASSERT_EQ(b - a, -a + b);
}

TYPED_TEST(FieldTest, UnaryMinusZeroIsZero) {
  const auto zero = TypeParam::Zero();
  EXPECT_EQ(-zero, zero);
}

TYPED_TEST(FieldTest, MinusMinusIsPlus) {
  const auto a = this->RandomElement();
  const auto b = this->RandomElement();
  ASSERT_EQ(a + b, a - (-b));
}

/*
  Checks that 0 = a * 0.
*/
TYPED_TEST(FieldTest, MultiplyByZero) {
  const auto a = this->RandomElement();
  const auto zero = TypeParam::Zero();
  EXPECT_EQ(zero, a * zero);
  EXPECT_EQ(zero, zero * a);
}

TYPED_TEST(FieldTest, InverseZero) { EXPECT_ASSERT(TypeParam::Zero().Inverse(), testing::_); }

TYPED_TEST(FieldTest, DivisionByZero) {
  const auto rand = this->RandomElement();
  const auto zero = TypeParam::Zero();
  EXPECT_ASSERT(rand / zero, testing::_);
  EXPECT_ASSERT(zero / zero, testing::_);
}

TYPED_TEST(FieldTest, InverseOne) { EXPECT_EQ(TypeParam::One().Inverse(), TypeParam::One()); }

TYPED_TEST(FieldTest, DivisionByOne) {
  const auto rand = RandomNonZeroElement<TypeParam>(&this->prng);
  const auto one = TypeParam::One();
  EXPECT_EQ(rand / one, rand);
  EXPECT_EQ(one / one, one);
}

/*
  Checks that a^i = Pow(a, i) for i < 10 and for i = 2^127.
*/
TYPED_TEST(FieldTest, Pow) {
  const auto a = this->RandomElement();
  auto power = TypeParam::One();
  for (int i = 0; i < 10; ++i) {
    ASSERT_EQ(power, Pow(a, i));
    power *= a;
  }
  power = a;
  for (int i = 0; i < 127; ++i) {
    power *= power;
  }
  EXPECT_EQ(power, Pow(a, static_cast<__uint128_t>(1) << 127));
}

// --- Test BatchPow ---

/*
  Verifies BatchPow doesn't crash for empty input.
*/
TYPED_TEST(FieldTest, BatchPowEmpty) {
  const auto base = this->RandomElement();
  std::vector<TypeParam> res;
  BatchPow<TypeParam>(base, {}, res);
  EXPECT_TRUE(res.empty());
}

/*
  Verifies BatchPow works as expected for a single element.
*/
TYPED_TEST(FieldTest, BatchPowSingle) {
  const auto base = this->RandomElement();
  const uint64_t exp = this->prng.UniformInt(0, 1000);
  std::vector<TypeParam> res({TypeParam::Zero()});
  BatchPow<TypeParam>(base, {exp}, res);
  EXPECT_EQ(Pow(base, exp), res[0]);
}

/*
  Verifies BatchPow works as expected for a single element with an exponent having a single bit on.
*/
TYPED_TEST(FieldTest, BatchPowSinglePow2) {
  const auto base = this->RandomElement();
  const uint64_t exp = Pow2(this->prng.UniformInt(0, 10));
  std::vector<TypeParam> res({TypeParam::Zero()});
  BatchPow<TypeParam>(base, {exp}, res);
  EXPECT_EQ(Pow(base, exp), res[0]);
}

/*
  Verifies BatchPow returns a vector of ones for zero exponent.
*/
TYPED_TEST(FieldTest, BatchPowZeroExponents) {
  const auto base = RandomNonZeroElement<TypeParam>(&this->prng);
  const size_t size = this->prng.UniformInt(1, 10);
  const std::vector<uint64_t> exp(size, 0);
  std::vector<TypeParam> res(size, TypeParam::Zero());
  BatchPow<TypeParam>(base, exp, res);
  EXPECT_EQ(std::vector<TypeParam>(size, TypeParam::One()), res);
}

/*
  Verifies BatchPow works as expected when some of the exponents are zero.
*/
TYPED_TEST(FieldTest, BatchPowSomeZeroExponents) {
  const auto base = this->RandomElement();
  const size_t size = 4;
  const std::vector<uint64_t> exp({1, 0, 3, 4});
  std::vector<TypeParam> res(size, this->RandomElement());
  BatchPow<TypeParam>(base, exp, res);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(Pow(base, exp[i]), res[i]);
  }
}

/*
  Verifies BatchPowEmpty with zero base returns one for zero exponent and zero for positive
  exponents.
*/
TYPED_TEST(FieldTest, BatchPowZeroBase) {
  const auto base = TypeParam::Zero();
  const size_t size = this->prng.UniformInt(0, 10);
  std::vector<uint64_t> exp = this->prng.template UniformIntVector<uint64_t>(0, 10000, size);
  exp.push_back(0);
  std::vector<TypeParam> expected_output;
  expected_output.reserve(size + 1);
  for (const uint64_t e : exp) {
    expected_output.push_back(e == 0 ? TypeParam::One() : TypeParam::Zero());
  }
  std::vector<TypeParam> res(size + 1, this->RandomElement());
  BatchPow<TypeParam>(base, exp, res);
  EXPECT_EQ(expected_output, res);
}

/*
  Verifies BatchPowEmpty with zero base and zero exponent returns a vector of ones.
*/
TYPED_TEST(FieldTest, BatchPowZeroBaseZeroExp) {
  const auto base = TypeParam::Zero();
  const size_t size = this->prng.UniformInt(0, 10);
  const std::vector<uint64_t> exp(size, 0);
  const std::vector<TypeParam> expected_output(size, TypeParam::One());
  std::vector<TypeParam> res(size, this->RandomElement());
  BatchPow<TypeParam>(base, exp, res);
  EXPECT_EQ(expected_output, res);
}

/*
  Verifies BatchPow with a random input.
*/
TYPED_TEST(FieldTest, BatchPowRandom_1) {
  const auto base = this->RandomElement();
  const size_t size = this->prng.UniformInt(0, 10);
  const std::vector<uint64_t> exp = this->prng.template UniformIntVector<uint64_t>(0, 10000, size);
  std::vector<TypeParam> res(size, TypeParam::Zero());
  BatchPow<TypeParam>(base, exp, res);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(Pow(base, exp[i]), res[i]);
  }
}

/*
  Tests the version of BatchPow that returns the result.
*/
TYPED_TEST(FieldTest, BatchPowRandom_2) {
  const auto base = this->RandomElement();
  const size_t size = this->prng.UniformInt(0, 10);
  const std::vector<uint64_t> exp = this->prng.template UniformIntVector<uint64_t>(0, 10000, size);
  const std::vector<TypeParam> res = BatchPow<TypeParam>(base, exp);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(Pow(base, exp[i]), res[i]);
  }
}

TYPED_TEST(FieldTest, UninitializedFieldElementArray) {
  auto res = UninitializedFieldElementArray<TypeParam, 4>();
  static_assert(
      std::is_same<decltype(res), std::array<TypeParam, 4>>::value,
      "UninitializedFieldElementArray returned an unexpected type.");
}

TYPED_TEST(FieldTest, ToFromBytes) {
  const auto a = this->RandomElement();
  std::vector<std::byte> a_bytes(a.SizeInBytes());
  a.ToBytes(a_bytes);
  const auto b = TypeParam::FromBytes(a_bytes);
  ASSERT_EQ(a, b);
}

TYPED_TEST(FieldTest, ToFromString) {
  std::stringstream s;
  const auto a = this->RandomElement();
  s << a;
  const auto b = TypeParam::FromString(s.str());
  ASSERT_EQ(a, b);
}

class BaseFieldTest : public ::testing::Test {
 public:
  Prng prng;
  BaseFieldElement RandomElement() { return BaseFieldElement::RandomElement(&prng); }
};

TEST_F(BaseFieldTest, InnerProduct) {
  const std::array<BaseFieldElement, 3> vec0{BaseFieldElement::Zero(), BaseFieldElement::One(),
                                             BaseFieldElement::FromUint(2)};
  const std::array<BaseFieldElement, 3> vec1{BaseFieldElement::One(), BaseFieldElement::FromUint(2),
                                             BaseFieldElement::FromUint(3)};
  EXPECT_EQ(BaseFieldElement::FromUint(8), InnerProduct<3>(vec0, vec1));
}

TEST_F(BaseFieldTest, InnerProductWithZero) {
  const std::array<BaseFieldElement, 2> zero_vec{BaseFieldElement::Zero(),
                                                 BaseFieldElement::Zero()};
  const std::array<BaseFieldElement, 2> vec{this->RandomElement(), this->RandomElement()};
  EXPECT_EQ(BaseFieldElement::Zero(), InnerProduct<2>(vec, zero_vec));
}

TEST_F(BaseFieldTest, InnerProductWithOne) {
  const std::array<BaseFieldElement, 2> one_vec{BaseFieldElement::One(), BaseFieldElement::One()};
  const std::array<BaseFieldElement, 2> vec{this->RandomElement(), this->RandomElement()};
  EXPECT_EQ(vec[0] + vec[1], InnerProduct<2>(one_vec, vec));
}

TEST_F(BaseFieldTest, LinearTransformation) {
  const std::array<BaseFieldElement, 2> row0{BaseFieldElement::Zero(), BaseFieldElement::One()};
  const std::array<BaseFieldElement, 2> row1{BaseFieldElement::FromUint(2),
                                             BaseFieldElement::FromUint(3)};
  const std::array<std::array<BaseFieldElement, 2>, 2> matrix{row0, row1};
  const std::array<BaseFieldElement, 2> vec{BaseFieldElement::One(), BaseFieldElement::FromUint(2)};
  std::array<BaseFieldElement, 2> output({BaseFieldElement::Zero(), BaseFieldElement::Zero()});
  const std::array<BaseFieldElement, 2> expected_output(
      {BaseFieldElement::FromUint(2), BaseFieldElement::FromUint(8)});
  LinearTransformation<2>(matrix, vec, &output);
  EXPECT_EQ(expected_output, output);
}

TEST_F(BaseFieldTest, LinearTransformationZeroVec) {
  const std::array<BaseFieldElement, 2> row0{this->RandomElement(), this->RandomElement()};
  const std::array<BaseFieldElement, 2> row1{this->RandomElement(), this->RandomElement()};
  const std::array<std::array<BaseFieldElement, 2>, 2> matrix{row0, row1};
  const std::array<BaseFieldElement, 2> zero_vec{BaseFieldElement::Zero(),
                                                 BaseFieldElement::Zero()};
  std::array<BaseFieldElement, 2> output({BaseFieldElement::One(), BaseFieldElement::One()});
  LinearTransformation<2>(matrix, zero_vec, &output);
  EXPECT_EQ(zero_vec, output);
}

TEST_F(BaseFieldTest, LinearTransformationZeroMatrix) {
  const std::array<BaseFieldElement, 2> zero_vec{BaseFieldElement::Zero(),
                                                 BaseFieldElement::Zero()};
  const std::array<std::array<BaseFieldElement, 2>, 2> matrix{zero_vec, zero_vec};
  const std::array<BaseFieldElement, 2> vec{this->RandomElement(), this->RandomElement()};
  std::array<BaseFieldElement, 2> output({BaseFieldElement::One(), BaseFieldElement::One()});
  LinearTransformation<2>(matrix, vec, &output);
  EXPECT_EQ(zero_vec, output);
}

TEST_F(BaseFieldTest, LinearTransformationIdentityMatrix) {
  const std::array<BaseFieldElement, 2> e1{BaseFieldElement::One(), BaseFieldElement::Zero()};
  const std::array<BaseFieldElement, 2> e2{BaseFieldElement::Zero(), BaseFieldElement::One()};
  const std::array<std::array<BaseFieldElement, 2>, 2> matrix{e1, e2};
  const std::array<BaseFieldElement, 2> vec{this->RandomElement(), this->RandomElement()};
  std::array<BaseFieldElement, 2> output({BaseFieldElement::Zero(), BaseFieldElement::Zero()});
  LinearTransformation<2>(matrix, vec, &output);
  EXPECT_EQ(vec, output);
}

// --- Test generator and prime factors ---

/*
  Verifies that GetSubGroupGenerator(n) generates a group of size n.
*/
TYPED_TEST(FieldTest, SubGroupGenerator) {
  EXPECT_ASSERT(GetSubGroupGenerator(6), testing::HasSubstr("Subgroup size must be a power of 2."));
  // Verifies the generator generates (n - 1) elements different from the identity and the identity
  // element.
  const uint64_t n = Pow2(this->prng.UniformInt(0, 20));
  const BaseFieldElement generator = GetSubGroupGenerator(n);
  auto field_element = TypeParam::One();
  for (uint64_t exp = 1; exp < n; ++exp) {
    field_element *= generator;
    ASSERT_NE(TypeParam::One(), field_element);
  }
  ASSERT_EQ(TypeParam::One(), field_element * generator);
}

/*
  Tests that PrimeFactors() returns all the prime factors of the multiplicative group's size.
*/
TEST_F(BaseFieldTest, Factors) {
  const auto factors = BaseFieldElement::PrimeFactors();
  __uint64_t cur = BaseFieldElement::FieldSize() - 1;
  // Divide by the factors as much as possible and ensure the result is 1.
  for (const uint64_t factor : factors) {
    ASSERT_NE(factor, 0);
    ASSERT_NE(factor, 1);
    // Make sure the factor divides the group size at least once.
    __uint64_t remainder;
    remainder = cur % factor;
    ASSERT_EQ(remainder, 0);
    // Keep dividing while the quotient is divisible by the factor.
    while (remainder == 0) {
      cur = cur / factor;
      remainder = cur % factor;
    }
  }
  ASSERT_EQ(1, cur);
}

/*
  Tests that a generator g of a field generates its multiplicative group G by:
  1. Checking that g^|G| = 1.
  2. Checking that for every prime factor p of |G|, g^(|G|/p) != 1.
  Assuming that all prime factors are checked (which we test for in a separate test), this is
  equivalent to checking that g^1, g^2, g^3,...,g^|G| is in fact all of G.
*/
TEST_F(BaseFieldTest, Generator) {
  const auto factors = BaseFieldElement::PrimeFactors();
  uint64_t group_size = BaseFieldElement::FieldSize() - 1;
  // Check that g^|G| = 1.
  BaseFieldElement raised_to_max_power = Pow(BaseFieldElement::Generator(), group_size);
  ASSERT_EQ(raised_to_max_power, BaseFieldElement::One());
  //  Check that for every prime factor p of |G|, g^(|G|/p) != 1.
  for (uint64_t factor : factors) {
    uint64_t quotient, remainder;
    quotient = group_size / factor;
    remainder = group_size % factor;
    ASSERT_EQ(remainder, 0);
    BaseFieldElement raised = Pow(BaseFieldElement::Generator(), quotient);
    ASSERT_NE(raised, BaseFieldElement::One());
  }
}

// --- Test field axioms ---

TYPED_TEST(FieldTest, OneIsNotZero) { EXPECT_NE(TypeParam::One(), TypeParam::Zero()); }

TYPED_TEST(FieldTest, AdditiveIdentity) {
  const auto r = this->RandomElement();
  ASSERT_EQ(r, r + TypeParam::Zero());
}

TYPED_TEST(FieldTest, MultiplicativeIdentity) {
  const auto r = this->RandomElement();
  ASSERT_EQ(r, r * TypeParam::One());
}

TYPED_TEST(FieldTest, AdditiveInverse) {
  const auto r = this->RandomElement();
  auto r_inverse = -r;
  ASSERT_EQ(TypeParam::Zero(), r + r_inverse);
}

/*
  This is aimed to play a bit with -1, hopefully catching mistakes in implementations.
  We check that:
    1 + (-1) = 0
    (-1) * (-1) = 1
    1 / (-1) = (-1).
*/
TYPED_TEST(FieldTest, MinusOneTests) {
  const auto minus_one = TypeParam::Zero() - TypeParam::One();
  EXPECT_EQ(TypeParam::Zero(), TypeParam::One() + minus_one);
  EXPECT_EQ(TypeParam::One(), minus_one * minus_one);
  EXPECT_EQ(minus_one, minus_one.Inverse());
}

TYPED_TEST(FieldTest, MultiplicativeInverse) {
  const auto r = RandomNonZeroElement<TypeParam>(&this->prng);
  const auto r_inverse = r.Inverse();
  ASSERT_EQ(TypeParam::One(), r * r_inverse);
}

TYPED_TEST(FieldTest, AdditiveCommutativity) {
  const auto a = this->RandomElement();
  const auto b = this->RandomElement();
  ASSERT_EQ(a + b, b + a);
}

TYPED_TEST(FieldTest, MultiplicativeCommutativity) {
  const auto a = this->RandomElement();
  const auto b = this->RandomElement();
  ASSERT_EQ(a * b, b * a);
}

TYPED_TEST(FieldTest, AdditiveAssociativity) {
  const auto a = this->RandomElement();
  const auto b = this->RandomElement();
  const auto c = this->RandomElement();
  ASSERT_EQ((a + b) + c, a + (b + c));
}

TYPED_TEST(FieldTest, MultiplicativeAssociativity) {
  const auto a = this->RandomElement();
  const auto b = this->RandomElement();
  const auto c = this->RandomElement();
  ASSERT_EQ((a * b) * c, a * (b * c));
}

TYPED_TEST(FieldTest, Distributivity) {
  const auto a = this->RandomElement();
  const auto b = this->RandomElement();
  const auto c = this->RandomElement();
  ASSERT_EQ(a * (b + c), a * b + a * c);
}

}  // namespace
}  // namespace starkware
