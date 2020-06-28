#include "starkware/algebra/domains/evaluation_domain.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(EvaluationDomainTests, CosetSize1) {
  EXPECT_ASSERT(EvaluationDomain(1, 1), HasSubstr("trace_size must be > 1"));
}

TEST(EvaluationDomainTests, TwoCosets) {
  const size_t coset_size = 2;
  const size_t n_cosets = 2;
  const size_t domain_size = coset_size * n_cosets;

  const BaseFieldElement cosets_offsets_generator = GetSubGroupGenerator(domain_size);
  const BaseFieldElement field_generator = BaseFieldElement::Generator();
  const BaseFieldElement group_generator = GetSubGroupGenerator(coset_size);

  const EvaluationDomain domain(coset_size, n_cosets);
  EXPECT_EQ(domain.Size(), domain_size);
  EXPECT_EQ(domain.ElementByIndex(0, 0), field_generator);
  EXPECT_EQ(domain.ElementByIndex(0, 1), field_generator * group_generator);
  EXPECT_EQ(domain.ElementByIndex(1, 0), field_generator * cosets_offsets_generator);
  EXPECT_EQ(
      domain.ElementByIndex(1, 1), field_generator * cosets_offsets_generator * group_generator);
  EXPECT_ASSERT(domain.ElementByIndex(0, 2), HasSubstr("Group index is out of range."));
}

TEST(EvaluationDomainTests, ZeroCosets) {
  Prng prng;
  const size_t coset_size = Pow2(prng.UniformInt(1, 5));
  const size_t n_cosets = 0;
  EXPECT_ASSERT(EvaluationDomain(coset_size, n_cosets), HasSubstr("n_cosets must be a power of 2"));
}

TEST(EvaluationDomainTests, Random) {
  Prng prng;
  const size_t log_coset_size = prng.UniformInt(1, 5);
  const size_t coset_size = Pow2(log_coset_size);
  const size_t n_cosets = Pow2(prng.UniformInt(1, 5));
  const EvaluationDomain domain(coset_size, n_cosets);

  const size_t domain_size = coset_size * n_cosets;
  const BaseFieldElement cosets_offsets_generator = GetSubGroupGenerator(domain_size);

  EXPECT_EQ(domain.Size(), domain_size);
  EXPECT_ASSERT(domain.ElementByIndex(n_cosets, 0), HasSubstr("Coset index is out of range."));

  BaseFieldElement curr_element = BaseFieldElement::Generator();
  for (size_t coset = 0; coset < n_cosets; ++coset) {
    for (size_t g = 0; g < coset_size; ++g) {
      size_t domain_index = BitReverse(g, log_coset_size);
      ASSERT_EQ(curr_element, domain.ElementByIndex(coset, domain_index))
          << "coset = " << coset << ", g = " << g << ", coset_size = " << coset_size
          << ", n_cosets = " << n_cosets << ".";
      curr_element = curr_element * domain.TraceGenerator();
    }
    curr_element = curr_element * cosets_offsets_generator;
  }
}

}  // namespace
}  // namespace starkware
