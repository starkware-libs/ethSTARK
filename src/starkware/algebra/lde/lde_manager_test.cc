#include "starkware/algebra/lde/lde_manager.h"

#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "starkware/algebra/domains/coset.h"
#include "starkware/algebra/fft/multiplicative_group_ordering.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

using testing::ElementsAreArray;

void GetEvaluationDegreeTest(MultiplicativeGroupOrdering order) {
  Prng prng;

  const bool is_natural_order = (order == MultiplicativeGroupOrdering::kNaturalOrder);
  const size_t log_domain_size = 4;
  const uint64_t domain_size = Pow2(log_domain_size);
  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const Coset domain(domain_size, offset);
  for (int64_t degree = -1; degree < static_cast<int64_t>(domain_size); ++degree) {
    const auto coefs = prng.RandomFieldElementVector<BaseFieldElement>(degree + 1);
    std::vector<BaseFieldElement> src;
    src.reserve(domain_size);
    for (auto x : domain.GetElements(order)) {
      src.push_back(HornerEval(x, coefs));
    }
    auto lde_manager = LdeManager<BaseFieldElement>(domain, is_natural_order);
    lde_manager.AddEvaluation(gsl::span<const BaseFieldElement>(src));

    EXPECT_EQ(lde_manager.GetEvaluationDegree(0), degree);
  }
}

TEST(LdeManagerTest, GetEvaluationDegree) {
  GetEvaluationDegreeTest(MultiplicativeGroupOrdering::kBitReversedOrder);
  GetEvaluationDegreeTest(MultiplicativeGroupOrdering::kNaturalOrder);
}

void GetDomainSizeTest(MultiplicativeGroupOrdering order) {
  Prng prng;

  const size_t log_domain_size = 4;
  const uint64_t domain_size = Pow2(log_domain_size);
  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const Coset domain(domain_size, offset);
  const bool is_natural_order = (order == MultiplicativeGroupOrdering::kNaturalOrder);
  const auto lde_manager = LdeManager<BaseFieldElement>(domain, is_natural_order);

  auto domain_size_from_lde = lde_manager.GetDomainSize();
  EXPECT_EQ(domain_size_from_lde, domain_size);
}

TEST(LdeManagerTest, GetDomainSize) {
  GetDomainSizeTest(MultiplicativeGroupOrdering::kBitReversedOrder);
  GetDomainSizeTest(MultiplicativeGroupOrdering::kNaturalOrder);
}

void TestLde(MultiplicativeGroupOrdering order) {
  Prng prng;

  const size_t log_domain_size = 4;
  const uint64_t domain_size = Pow2(log_domain_size);
  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const Coset domain(domain_size, offset);

  const auto coefs = prng.RandomFieldElementVector<BaseFieldElement>(domain_size);
  std::vector<BaseFieldElement> src;
  src.reserve(domain_size);
  for (auto x : domain.GetElements(order)) {
    src.push_back(HornerEval(x, coefs));
  }

  const bool is_natural_order = (order == MultiplicativeGroupOrdering::kNaturalOrder);
  auto lde_manager = LdeManager<BaseFieldElement>(domain, is_natural_order);
  lde_manager.AddEvaluation(gsl::span<const BaseFieldElement>(src));

  std::vector<BaseFieldElement> res = BaseFieldElement::UninitializedVector(domain_size);

  const Coset shifted_domain(domain_size, BaseFieldElement::RandomElement(&prng));
  auto out_span = gsl::make_span(res);
  lde_manager.EvalOnCoset(shifted_domain.Offset(), {out_span});

  std::vector<BaseFieldElement> concurrent_res = BaseFieldElement::UninitializedVector(domain_size);
  out_span = gsl::make_span(concurrent_res);
  auto task_manager = TaskManager::CreateInstanceForTesting();
  lde_manager.EvalOnCoset(shifted_domain.Offset(), {out_span}, &task_manager);

  for (const auto& [x, result, concurrent_result] :
       iter::zip(shifted_domain.GetElements(order), res, concurrent_res)) {
    BaseFieldElement expected = HornerEval(x, coefs);
    ASSERT_EQ(expected, result);
    ASSERT_EQ(expected, concurrent_result);
  }
}

TEST(LdeManagerTest, Lde) {
  TestLde(MultiplicativeGroupOrdering::kBitReversedOrder);
  TestLde(MultiplicativeGroupOrdering::kNaturalOrder);
}

void LdeWithOffsetTest(MultiplicativeGroupOrdering order) {
  Prng prng;

  const size_t log_domain_size = 4;
  const uint64_t domain_size = Pow2(log_domain_size);
  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const Coset domain(domain_size, offset);
  const bool is_natural_order = (order == MultiplicativeGroupOrdering::kNaturalOrder);
  auto lde_manager = LdeManager<BaseFieldElement>(domain, is_natural_order);

  const auto coefs = prng.RandomFieldElementVector<BaseFieldElement>(domain_size);
  lde_manager.AddFromCoefficients(coefs);

  std::vector<BaseFieldElement> res = BaseFieldElement::UninitializedVector(domain_size);
  auto out_span = gsl::make_span(res);
  const Coset shifted_domain(domain_size, BaseFieldElement::RandomElement(&prng));
  lde_manager.EvalOnCoset(shifted_domain.Offset(), {out_span});

  lde_manager.AddEvaluation(gsl::span<const BaseFieldElement>(res));

  // FFT + IFFT with different offsets should give the different coefficients.
  EXPECT_NE(lde_manager.GetCoefficients(0), lde_manager.GetCoefficients(1));
}

TEST(LdeManagerTest, LdeWithOffset) {
  LdeWithOffsetTest(MultiplicativeGroupOrdering::kBitReversedOrder);
  LdeWithOffsetTest(MultiplicativeGroupOrdering::kNaturalOrder);
}

TEST(LdeManagerTest, Identity) {
  Prng prng;

  const size_t log_domain_size = 4;
  const uint64_t domain_size = Pow2(log_domain_size);
  const BaseFieldElement offset = BaseFieldElement::RandomElement(&prng);
  const Coset coset(domain_size, offset);

  const auto vec = prng.RandomFieldElementVector<BaseFieldElement>(domain_size);
  auto lde_manager = MakeLdeManager<BaseFieldElement>(coset);

  lde_manager->AddEvaluation(std::vector<BaseFieldElement>(vec));
  auto res = BaseFieldElement::UninitializedVector(vec.size());
  lde_manager->EvalOnCoset(offset, std::vector<gsl::span<BaseFieldElement>>{gsl::make_span(res)});
  ASSERT_EQ(res, vec);
}

TEST(LdeManagerTest, kBitReversedOrderResult) {
  Prng prng;

  const size_t log_domain_size = 4;
  const uint64_t domain_size = Pow2(log_domain_size);

  const auto source_eval_offset = BaseFieldElement::RandomElement(&prng);
  const Coset coset(domain_size, source_eval_offset);

  auto lde_manager = MakeLdeManager<BaseFieldElement>(coset, true);

  auto vec = prng.RandomFieldElementVector<BaseFieldElement>(domain_size);
  lde_manager->AddEvaluation(std::vector<BaseFieldElement>(vec));

  // lde_manager_rev expects the input in reverse order.
  auto lde_manager_rev = MakeLdeManager<BaseFieldElement>(coset, false);
  vec = BitReverseVector<BaseFieldElement>(vec);
  lde_manager_rev->AddEvaluation(std::move(vec));

  auto expected = BaseFieldElement::UninitializedVector(domain_size);
  auto expected_rev = BaseFieldElement::UninitializedVector(domain_size);

  const auto eval_offset = BaseFieldElement::RandomElement(&prng);

  auto out_span = gsl::make_span(expected);
  lde_manager->EvalOnCoset(eval_offset, {out_span});
  out_span = gsl::make_span(expected_rev);
  lde_manager_rev->EvalOnCoset(eval_offset, {out_span});
  EXPECT_EQ(expected, BitReverseVector<BaseFieldElement>(expected_rev));
}

void EvalAtPointTest(const size_t log_domain_size, MultiplicativeGroupOrdering order) {
  Prng prng;

  const uint64_t domain_size = Pow2(log_domain_size);
  const auto source_eval_offset = BaseFieldElement::RandomElement(&prng);
  const Coset coset(domain_size, source_eval_offset);
  const bool is_natural_order = (order == MultiplicativeGroupOrdering::kNaturalOrder);
  auto lde_manager = MakeLdeManager<BaseFieldElement>(coset, is_natural_order);

  auto vec = prng.RandomFieldElementVector<BaseFieldElement>(domain_size);
  if (!is_natural_order) {
    // lde_manager_rev expects the input in reverse order.
    vec = BitReverseVector<BaseFieldElement>(vec);
  }
  lde_manager->AddEvaluation(std::move(vec));

  auto expected_res = BaseFieldElement::UninitializedVector(domain_size);
  auto out_span = gsl::make_span(expected_res);
  const auto eval_offset = BaseFieldElement::RandomElement(&prng);
  lde_manager->EvalOnCoset(eval_offset, {out_span});

  if (!is_natural_order) {
    // Test below expects natural order.
    expected_res = BitReverseVector<BaseFieldElement>(expected_res);
  }

  BaseFieldElement w = coset.Generator();

  std::vector<BaseFieldElement> points;
  BaseFieldElement offset = eval_offset;
  points.reserve(domain_size);
  for (size_t i = 0; i < domain_size; i++) {
    points.push_back(offset);
    offset = offset * w;
  }

  std::vector<BaseFieldElement> results(domain_size, BaseFieldElement::Zero());
  lde_manager->template EvalAtPoints<BaseFieldElement>(0, points, results);
  EXPECT_THAT(results, ElementsAreArray(expected_res));

  std::vector<BaseFieldElement> results2(domain_size, BaseFieldElement::Zero());
  lde_manager->EvalAtPoints(0, gsl::span<const BaseFieldElement>(points), gsl::make_span(results2));
  EXPECT_THAT(results2, ElementsAreArray(expected_res));
}

TEST(LdeManagerTest, EvalAtPoint) {
  EvalAtPointTest(4, MultiplicativeGroupOrdering::kNaturalOrder);
  EvalAtPointTest(1, MultiplicativeGroupOrdering::kNaturalOrder);
  EvalAtPointTest(0, MultiplicativeGroupOrdering::kNaturalOrder);

  EvalAtPointTest(4, MultiplicativeGroupOrdering::kBitReversedOrder);
  EvalAtPointTest(1, MultiplicativeGroupOrdering::kBitReversedOrder);
  EvalAtPointTest(0, MultiplicativeGroupOrdering::kBitReversedOrder);
}

void TestAddFromAndGetCoefficients(MultiplicativeGroupOrdering order) {
  Prng prng;
  const uint64_t domain_size = 8;

  const BaseFieldElement source_eval_offset = BaseFieldElement::RandomElement(&prng);
  const Coset coset(domain_size, source_eval_offset);
  const bool is_natural_order = (order == MultiplicativeGroupOrdering::kNaturalOrder);
  auto lde_manager = MakeLdeManager<BaseFieldElement>(coset, is_natural_order);

  const auto coefs = prng.RandomFieldElementVector<BaseFieldElement>(domain_size);
  lde_manager->AddFromCoefficients(coefs);

  // Test that GetCoefficients() returns the same coefficients given to AddFromCoefficients().
  auto coefs2 = lde_manager->GetCoefficients(0);
  EXPECT_THAT(coefs2, ElementsAreArray(coefs));

  // Test that coefficients are consistent with EvalAtPoints.
  const BaseFieldElement point = BaseFieldElement::RandomElement(&prng);
  BaseFieldElement output = BaseFieldElement::Zero();
  lde_manager->EvalAtPoints(0, gsl::make_span(&point, 1), gsl::make_span(&output, 1));

  BaseFieldElement expected_output =
      HornerEval(point, is_natural_order ? BitReverseVector<BaseFieldElement>(coefs) : coefs);
  EXPECT_EQ(expected_output, output);
}

TEST(LdeManagerTest, AddFromAndGetCoefficients) {
  TestAddFromAndGetCoefficients(MultiplicativeGroupOrdering::kNaturalOrder);
  TestAddFromAndGetCoefficients(MultiplicativeGroupOrdering::kBitReversedOrder);
}

}  // namespace
}  // namespace starkware
