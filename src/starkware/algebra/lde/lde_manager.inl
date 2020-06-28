#include "starkware/algebra/lde/lde_manager.h"

#include "third_party/cppitertools/zip.hpp"

#include "starkware/algebra/fft/fft.h"
#include "starkware/algebra/polynomials.h"

namespace starkware {

template <typename FieldElementT>
LdeManager<FieldElementT>::LdeManager(const Coset& coset, bool eval_in_natural_order)
    : coset_(coset), eval_in_natural_order_(eval_in_natural_order) {}

template <typename FieldElementT>
void LdeManager<FieldElementT>::AddEvaluation(gsl::span<const FieldElementT> evaluation) {
  AddEvaluation(std::vector<FieldElementT>(evaluation.begin(), evaluation.end()));
}

template <typename FieldElementT>
void LdeManager<FieldElementT>::AddEvaluation(std::vector<FieldElementT>&& evaluation) {
  Ifft<FieldElementT>(
      evaluation, evaluation, coset_.Generator(), coset_.Offset(), eval_in_natural_order_);

  // Normalize IFFT output.
  auto lde_size_inverse = FieldElementT::FromUint(evaluation.size()).Inverse();
  TaskManager::GetInstance().ParallelFor(
      evaluation.size(),
      [&evaluation, lde_size_inverse](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          evaluation[i] *= lde_size_inverse;
        }
      },
      evaluation.size());

  if (eval_in_natural_order_) {
    polynomials_natural_order_coefficients_vector_.push_back(
        BitReverseVector<FieldElementT>(evaluation));
  }
  polynomials_vector_.push_back(std::move(evaluation));
}

template <typename FieldElementT>
void LdeManager<FieldElementT>::EvalOnCoset(
    const BaseFieldElement& coset_offset,
    gsl::span<const gsl::span<FieldElementT>> evaluation_results, TaskManager* task_manager) const {
  ASSERT_RELEASE(
      polynomials_vector_.size() == evaluation_results.size(),
      "evaluation_results.size() must match number of LDEs.");

  for (const auto& column : evaluation_results) {
    ASSERT_RELEASE(column.size() == coset_.Size(), "Wrong column output size.");
  }

  if (coset_.Size() == 1) {
    // FFT cannot handle the case where coset_.Size() == 1.
    for (const auto& [polynomial, out_span] : iter::zip(polynomials_vector_, evaluation_results)) {
      out_span[0] = polynomial[0];
    }
    return;
  }

  task_manager->ParallelFor(
      polynomials_vector_.size(),
      [this, evaluation_results, &coset_offset](const TaskInfo& task_info) {
        const size_t idx = task_info.start_idx;
        Fft<FieldElementT>(
            polynomials_vector_[idx], evaluation_results[idx], coset_.Generator(), coset_offset,
            eval_in_natural_order_);
      });
}

template <typename FieldElementT>
void LdeManager<FieldElementT>::EvalOnCoset(
    const BaseFieldElement& coset_offset,
    gsl::span<const gsl::span<FieldElementT>> evaluation_results) const {
  EvalOnCoset(coset_offset, evaluation_results, &TaskManager::GetInstance());
}

template <typename FieldElementT>
void LdeManager<FieldElementT>::AddFromCoefficients(gsl::span<const FieldElementT> coefficients) {
  ASSERT_RELEASE(
      coefficients.size() == coset_.Size(),
      "The expected number of coefficients (" + std::to_string(coset_.Size()) +
          ") does not match the actual number of coefficients (" +
          std::to_string(coefficients.size()) + ").");
  if (eval_in_natural_order_) {
    polynomials_natural_order_coefficients_vector_.push_back(
        BitReverseVector<FieldElementT>(coefficients));
  }
  polynomials_vector_.push_back(
      std::vector<FieldElementT>(coefficients.begin(), coefficients.end()));
}

template <typename FieldElementT>
template <typename ExtFieldElementT>
void LdeManager<FieldElementT>::EvalAtPoints(
    size_t evaluation_idx, gsl::span<const ExtFieldElementT> points,
    gsl::span<ExtFieldElementT> outputs) const {
  ASSERT_RELEASE(evaluation_idx < polynomials_vector_.size(), "evaluation_idx out of range.");
  std::vector<ExtFieldElementT> fixed_points =
      std::vector<ExtFieldElementT>(points.begin(), points.end());
  const std::vector<FieldElementT>& polynomial_in_natural_order =
      eval_in_natural_order_ ? polynomials_natural_order_coefficients_vector_[evaluation_idx]
                             : polynomials_vector_[evaluation_idx];
  BatchHornerEval<ExtFieldElementT, FieldElementT>(
      fixed_points, polynomial_in_natural_order, outputs);
}

template <typename FieldElementT>
int64_t LdeManager<FieldElementT>::GetEvaluationDegree(size_t evaluation_idx) const {
  ASSERT_RELEASE(evaluation_idx < polynomials_vector_.size(), "evaluation_idx out of range.");
  const std::vector<FieldElementT>& polynomial_in_natural_order =
      eval_in_natural_order_ ? polynomials_natural_order_coefficients_vector_[evaluation_idx]
                             : polynomials_vector_[evaluation_idx];
  for (int64_t deg = polynomial_in_natural_order.size() - 1; deg >= 0; deg--) {
    if (polynomial_in_natural_order[deg] != FieldElementT::Zero()) {
      return deg;
    }
  }
  return -1;
}

template <typename FieldElementT>
gsl::span<const FieldElementT> LdeManager<FieldElementT>::GetCoefficients(
    size_t evaluation_idx) const {
  ASSERT_RELEASE(evaluation_idx < polynomials_vector_.size(), "evaluation_idx out of range.");
  return gsl::span<const FieldElementT>(polynomials_vector_[evaluation_idx]);
}

template <typename FieldElementT>
uint64_t LdeManager<FieldElementT>::GetDomainSize() const {
  return coset_.Size();
}

template <typename FieldElementT>
bool LdeManager<FieldElementT>::IsEvalNaturallyOrdered() const {
  return eval_in_natural_order_;
}

template <typename FieldElementT>
std::unique_ptr<LdeManager<FieldElementT>> MakeLdeManager(
    const Coset& source_domain_coset, bool eval_in_natural_order) {
  return std::make_unique<LdeManager<FieldElementT>>(source_domain_coset, eval_in_natural_order);
}

}  // namespace starkware
