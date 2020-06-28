#ifndef STARKWARE_ALGEBRA_LDE_LDE_MANAGER_MOCK_H_
#define STARKWARE_ALGEBRA_LDE_LDE_MANAGER_MOCK_H_

#include <vector>

#include "gmock/gmock.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/algebra/lde/lde_manager.h"

namespace starkware {

class LdeManagerMock : public LdeManager<BaseFieldElement> {
 public:
  explicit LdeManagerMock(uint64_t lde_size, BaseFieldElement offset)
      : LdeManager<BaseFieldElement>(Coset(lde_size, offset), false) {}

  /*
    Workaround. GMock has a problem with rvalue reference parameter (&&). So, we implement it and
    pass execution to AddEvaluation_rvr (rvr stands for rvalue reference).
  */
  void AddEvaluation(std::vector<BaseFieldElement>&& evaluation) override {
    AddEvaluation_rvr(evaluation);
  }
  MOCK_METHOD1(AddEvaluation_rvr, void(const std::vector<BaseFieldElement>& evaluation));
  MOCK_METHOD1(AddEvaluation, void(gsl::span<const BaseFieldElement> evaluation));
  MOCK_CONST_METHOD2(
      EvalOnCoset, void(
                       const BaseFieldElement& coset_offset,
                       gsl::span<const gsl::span<BaseFieldElement>> evaluation_results));
  MOCK_CONST_METHOD3(
      EvalAtPoints, void(
                        size_t evaluation_idx, gsl::span<const ExtensionFieldElement> points,
                        gsl::span<ExtensionFieldElement> outputs));
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_LDE_LDE_MANAGER_MOCK_H_
