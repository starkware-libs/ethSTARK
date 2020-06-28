#include "starkware/algebra/field_operations.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/bit_reversal.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

template <typename AirT>
void CompositionPolynomialImpl<AirT>::Builder::AddPeriodicColumn(
    PeriodicColumn column, size_t periodic_column_index) {
  ASSERT_RELEASE(
      !periodic_columns_[periodic_column_index].has_value(), "Cannot set periodic column twice.");
  periodic_columns_[periodic_column_index].emplace(std::move(column));
}

template <typename AirT>
CompositionPolynomialImpl<AirT> CompositionPolynomialImpl<AirT>::Builder::Build(
    MaybeOwnedPtr<const AirT> air, const BaseFieldElement& trace_generator,
    const uint64_t coset_size, gsl::span<const ExtensionFieldElement> random_coefficients,
    gsl::span<const uint64_t> point_exponents, gsl::span<const BaseFieldElement> shifts) {
  std::vector<PeriodicColumn> periodic_columns;
  periodic_columns.reserve(periodic_columns_.size());
  for (size_t i = 0; i < periodic_columns_.size(); ++i) {
    ASSERT_RELEASE(
        periodic_columns_[i].has_value(),
        "Uninitialized periodic column at index " + std::to_string(i) + ".");
    periodic_columns.push_back(std::move(periodic_columns_[i].value()));
  }
  return CompositionPolynomialImpl(
      std::move(air), trace_generator, coset_size, periodic_columns, random_coefficients,
      point_exponents, shifts);
}

template <typename AirT>
std::unique_ptr<CompositionPolynomialImpl<AirT>>
CompositionPolynomialImpl<AirT>::Builder::BuildUniquePtr(
    MaybeOwnedPtr<const AirT> air, const BaseFieldElement& trace_generator,
    const uint64_t coset_size, gsl::span<const ExtensionFieldElement> random_coefficients,
    gsl::span<const uint64_t> point_exponents, gsl::span<const BaseFieldElement> shifts) {
  return std::make_unique<CompositionPolynomialImpl<AirT>>(Build(
      std::move(air), trace_generator, coset_size, random_coefficients, point_exponents, shifts));
}

template <typename AirT>
CompositionPolynomialImpl<AirT>::CompositionPolynomialImpl(
    MaybeOwnedPtr<const AirT> air, BaseFieldElement trace_generator, uint64_t coset_size,
    std::vector<PeriodicColumn> periodic_columns,
    gsl::span<const ExtensionFieldElement> coefficients, gsl::span<const uint64_t> point_exponents,
    gsl::span<const BaseFieldElement> shifts)
    : air_(std::move(air)),
      trace_generator_(trace_generator),
      coset_size_(coset_size),
      periodic_columns_(std::move(periodic_columns)),
      coefficients_(coefficients.begin(), coefficients.end()),
      point_exponents_(point_exponents.begin(), point_exponents.end()),
      shifts_(shifts.begin(), shifts.end()) {
  ASSERT_RELEASE(
      coefficients.size() == air_->NumRandomCoefficients(), "Wrong number of coefficients.");
  ASSERT_RELEASE(
      IsPowerOfTwo(coset_size_), "Only cosets of size which is a power of two are supported.");
  ASSERT_RELEASE(
      Pow(trace_generator_, coset_size_) == BaseFieldElement::One(),
      "The provided generator does not generate a group of the expected size.");
}

template <typename AirT>
template <typename FieldElementT>
ExtensionFieldElement CompositionPolynomialImpl<AirT>::EvalAtPointImpl(
    const FieldElementT& point, gsl::span<const FieldElementT> neighbors,
    gsl::span<const ExtensionFieldElement> composition_neighbors) const {
  std::vector<FieldElementT> periodic_column_vals;
  periodic_column_vals.reserve(periodic_columns_.size());
  for (const PeriodicColumn& column : periodic_columns_) {
    periodic_column_vals.push_back(column.EvalAtPoint(point));
  }

  std::vector<FieldElementT> point_powers =
      FieldElementT::UninitializedVector(1 + point_exponents_.size());
  point_powers[0] = point;
  BatchPow(point, point_exponents_, gsl::make_span(point_powers).subspan(1));

  return air_->template ConstraintsEval<FieldElementT>(
      neighbors, composition_neighbors, periodic_column_vals, coefficients_, point_powers, shifts_);
}

template <typename AirT>
void CompositionPolynomialImpl<AirT>::EvalOnCosetBitReversedOutput(
    const BaseFieldElement& coset_offset,
    gsl::span<const gsl::span<const BaseFieldElement>> trace_lde,
    gsl::span<const gsl::span<const ExtensionFieldElement>> composition_trace_lde,
    gsl::span<ExtensionFieldElement> out_evaluation, uint64_t task_size) const {
  Neighbors neighbors(air_->GetMask(), trace_lde, composition_trace_lde);
  EvalOnCosetBitReversedOutput(coset_offset, neighbors, out_evaluation, task_size);
}

namespace composition_polynomial {
namespace details {

class CompositionPolynomialImplWorkerMemory {
  using PeriodicColumnIterator = typename PeriodicColumn::CosetEvaluation::Iterator;

 public:
  CompositionPolynomialImplWorkerMemory(size_t periodic_columns_size, size_t point_powers_size)
      : periodic_column_vals(BaseFieldElement::UninitializedVector(periodic_columns_size)),
        point_powers(BaseFieldElement::UninitializedVector(point_powers_size)) {
    periodic_columns_iter.reserve(periodic_columns_size);
  }

  // Iterators for the evaluations of the periodic columns on a coset.
  std::vector<PeriodicColumnIterator> periodic_columns_iter;
  // Pre-allocated space for periodic column results.
  std::vector<BaseFieldElement> periodic_column_vals;
  // Pre-allocated space for the powers of the evaluation point.
  std::vector<BaseFieldElement> point_powers;
};

}  // namespace details
}  // namespace composition_polynomial

template <typename AirT>
void CompositionPolynomialImpl<AirT>::EvalOnCosetBitReversedOutput(
    const BaseFieldElement& coset_offset, const Neighbors& neighbors,
    gsl::span<ExtensionFieldElement> out_evaluation, uint64_t task_size) const {
  // Input verification.
  ASSERT_RELEASE(
      out_evaluation.size() == coset_size_, "Output span size does not match the coset size.");

  ASSERT_RELEASE(
      neighbors.CosetSize() == coset_size_,
      "Given neighbors iterator is not of the expected length.");

  // Precompute useful constants.
  const size_t log_coset_size = SafeLog2(coset_size_);
  const uint_fast64_t n_tasks = DivCeil(coset_size_, task_size);

  // Prepare offset for each task.
  std::vector<BaseFieldElement> algebraic_offsets;
  algebraic_offsets.reserve(n_tasks);
  BaseFieldElement point = coset_offset;
  const BaseFieldElement point_multiplier = Pow(trace_generator_, task_size);
  for (uint64_t i = 0; i < n_tasks; ++i) {
    algebraic_offsets.push_back(point);
    point *= point_multiplier;
  }

  // Prepare #threads workers.
  using WorkerMemoryT = composition_polynomial::details::CompositionPolynomialImplWorkerMemory;
  TaskManager& task_manager = TaskManager::GetInstance();
  std::vector<WorkerMemoryT> worker_mem;
  worker_mem.reserve(task_manager.GetNumThreads());
  for (size_t i = 0; i < task_manager.GetNumThreads(); ++i) {
    worker_mem.emplace_back(periodic_columns_.size(), 1 + point_exponents_.size());
  }

  // Prepare iterator for each periodic column.
  std::vector<typename PeriodicColumn::CosetEvaluation> periodic_column_cosets;
  periodic_column_cosets.reserve(periodic_columns_.size());
  for (const PeriodicColumn& column : periodic_columns_) {
    periodic_column_cosets.emplace_back(column.GetCoset(coset_offset, coset_size_));
  }

  // Evaluate on coset.
  const std::vector<BaseFieldElement> gen_powers = BatchPow(trace_generator_, point_exponents_);
  task_manager.ParallelFor(
      n_tasks, [this, &algebraic_offsets, &periodic_column_cosets, &gen_powers, &worker_mem,
                &neighbors, &out_evaluation, log_coset_size, task_size](const TaskInfo& task_info) {
        const uint64_t initial_point_idx = task_size * task_info.start_idx;
        BaseFieldElement point = algebraic_offsets[task_info.start_idx];
        WorkerMemoryT& wm = worker_mem[TaskManager::GetWorkerId()];

        // Compute point powers.
        wm.point_powers[0] = point;
        BatchPow(point, point_exponents_, gsl::make_span(wm.point_powers).subspan(1));

        // Initialize periodic columns interators.
        wm.periodic_columns_iter.clear();
        for (const auto& column_coset : periodic_column_cosets) {
          wm.periodic_columns_iter.push_back(column_coset.begin() + initial_point_idx);
        }

        typename Neighbors::Iterator neighbors_iter = neighbors.begin();
        neighbors_iter += static_cast<size_t>(initial_point_idx);

        const size_t actual_task_size = std::min(task_size, coset_size_ - initial_point_idx);
        const size_t end_of_coset_index = initial_point_idx + actual_task_size;

        for (size_t point_idx = initial_point_idx; point_idx < end_of_coset_index; ++point_idx) {
          ASSERT_RELEASE(
              neighbors_iter != neighbors.end(),
              "neighbors_iter reached the end of the iterator unexpectedly.");
          // Evaluate periodic columns.
          for (size_t i = 0; i < periodic_columns_.size(); ++i) {
            wm.periodic_column_vals[i] = *wm.periodic_columns_iter[i];
            ++wm.periodic_columns_iter[i];
          }

          // Evaluate on a single point.
          auto [neighbors_vals, composition_neighbors] = *neighbors_iter;  // NOLINT
          out_evaluation[BitReverse(point_idx, log_coset_size)] =
              air_->template ConstraintsEval<BaseFieldElement>(
                  neighbors_vals, composition_neighbors, wm.periodic_column_vals, coefficients_,
                  wm.point_powers, shifts_);

          // On the last iteration, skip the preperations for the next iterations.
          if (point_idx + 1 < end_of_coset_index) {
            // Advance evaluation point.
            point *= trace_generator_;
            wm.point_powers[0] = point;
            for (size_t i = 0; i < gen_powers.size(); ++i) {
              // Shift point_powers, x^k -> (g*x)^k.
              wm.point_powers[i + 1] *= gen_powers[i];
            }

            ++neighbors_iter;
          }
        }
      });
}

}  // namespace starkware
