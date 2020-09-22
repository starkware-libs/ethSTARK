#include "starkware/channel/annotation_scope.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/crypt_tools/blake2s_256.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(AnnotationScope, BasicTest) {
  Prng prng;
  ProverChannel prover_channel(Prng{});

  {
    AnnotationScope scope(&prover_channel, "STARK");
    {
      AnnotationScope scope(&prover_channel, "FRI");
      {
        AnnotationScope scope(&prover_channel, "Layer 1");
        {
          AnnotationScope scope(&prover_channel, "Commitment");
          Blake2s256 pcommitment1 = prng.RandomHash();
          prover_channel.SendCommitmentHash(pcommitment1, "First FRI layer");
        }
        {
          AnnotationScope scope(&prover_channel, "Eval point");
          ExtensionFieldElement ptest_field_element1 =
              prover_channel.ReceiveFieldElement("evaluation point");
          (void)ptest_field_element1;
          ExtensionFieldElement ptest_field_element2 =
              prover_channel.ReceiveFieldElement("2nd evaluation point");
          (void)ptest_field_element2;
        }
      }
      {
        AnnotationScope scope(&prover_channel, "Last Layer");
        {
          AnnotationScope scope(&prover_channel, "Commitment");
          const auto pexpected_last_layer_const = ExtensionFieldElement::RandomElement(&prng);

          prover_channel.SendFieldElement(pexpected_last_layer_const, "expected last layer const");
        }
        {
          AnnotationScope scope(&prover_channel, "Query");
          prover_channel.ReceiveNumber(8, "index #1");
          prover_channel.ReceiveNumber(8, "index #2");
        }
      }
      {
        AnnotationScope scope(&prover_channel, "Decommitment");
        for (size_t i = 0; i < 15; i++) {
          prover_channel.SendDecommitmentNode(prng.RandomHash(), "FRI layer");
        }
      }
    }
    std::ostringstream output;
    output << prover_channel;
    EXPECT_THAT(output.str(), HasSubstr("/STARK/FRI/Layer 1/Commitment"));
    EXPECT_THAT(output.str(), HasSubstr("/STARK/FRI/Layer 1/Eval point"));
    EXPECT_THAT(output.str(), HasSubstr("/STARK/FRI/Decommitment"));
  }
}

}  // namespace
}  // namespace starkware
