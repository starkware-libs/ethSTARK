#include <cstddef>
#include <string>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/base_field_element.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/channel.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

using testing::HasSubstr;

class ChannelTest : public ::testing::Test {
 public:
  Prng prng;
  const Prng channel_prng{};
  std::vector<std::byte> RandomByteVector(size_t length) { return prng.RandomByteVector(length); }
};

TEST_F(ChannelTest, SendingConsistentWithReceivingBytes) {
  ProverChannel prover_channel(this->channel_prng.Clone());
  const size_t num_bytes_prover_data_1 = 8;
  const size_t num_bytes_prover_data_2 = 4;
  std::vector<std::byte> prover_data1 = this->RandomByteVector(num_bytes_prover_data_1);
  prover_channel.SendBytes(gsl::make_span(prover_data1).as_span<std::byte>());
  std::vector<std::byte> prover_data2 = this->RandomByteVector(num_bytes_prover_data_2);
  prover_channel.SendBytes(gsl::make_span(prover_data2).as_span<std::byte>());

  VerifierChannel verifier_channel(this->channel_prng.Clone(), prover_channel.GetProof());

  std::vector<std::byte> verifier_data1(verifier_channel.ReceiveBytes(num_bytes_prover_data_1));
  EXPECT_EQ(verifier_data1, prover_data1);
  std::vector<std::byte> verifier_data2(verifier_channel.ReceiveBytes(num_bytes_prover_data_2));
  EXPECT_EQ(verifier_data2, prover_data2);
}

TEST_F(ChannelTest, ProofOfWork) {
  ProverChannel prover_channel(this->channel_prng.Clone());

  const size_t work_bits = 15;
  prover_channel.ApplyProofOfWork(work_bits);
  uint64_t pow_value = prover_channel.ReceiveNumber(Pow2(24));

  // Completeness.
  VerifierChannel verifier_channel(this->channel_prng.Clone(), prover_channel.GetProof());

  verifier_channel.ApplyProofOfWork(work_bits);
  EXPECT_EQ(verifier_channel.GetAndSendRandomNumber(Pow2(24)), pow_value);

  // Soundness.
  VerifierChannel verifier_channel_bad_1(this->channel_prng.Clone(), prover_channel.GetProof());
  EXPECT_ASSERT(
      verifier_channel_bad_1.ApplyProofOfWork(work_bits + 1), HasSubstr("Wrong proof of work"));

  VerifierChannel verifier_channel_bad2(this->channel_prng.Clone(), prover_channel.GetProof());

  // Note this fails with probability 2^{-14}.
  EXPECT_ASSERT(
      verifier_channel_bad2.ApplyProofOfWork(work_bits - 1), HasSubstr("Wrong proof of work"));

  // Check value was actually changed.
  ProverChannel nonpow_prover_channel(this->channel_prng.Clone());
  EXPECT_NE(nonpow_prover_channel.ReceiveNumber(Pow2(24)), pow_value);
}

TEST_F(ChannelTest, ProofOfWorkDependsOnState) {
  ProverChannel prover_channel_1(this->channel_prng.Clone());
  std::vector<std::byte> prover_data1 = this->RandomByteVector(8);
  prover_channel_1.SendBytes(gsl::make_span(prover_data1).as_span<std::byte>());

  const size_t work_bits = 15;
  prover_channel_1.ApplyProofOfWork(work_bits);
  uint64_t pow_value_1 = prover_channel_1.ReceiveNumber(Pow2(24));

  ProverChannel prover_channel_2(this->channel_prng.Clone());
  std::vector<std::byte> prover_data2 = this->RandomByteVector(8);
  prover_channel_2.SendBytes(gsl::make_span(prover_data2).as_span<std::byte>());

  prover_channel_2.ApplyProofOfWork(work_bits);
  uint64_t pow_value_2 = prover_channel_2.ReceiveNumber(Pow2(24));

  EXPECT_NE(pow_value_1, pow_value_2);
}

TEST_F(ChannelTest, ProofOfWorkZeroBits) {
  ProverChannel prover_channel_1(this->channel_prng.Clone());

  prover_channel_1.ApplyProofOfWork(0);
  uint64_t pow_value_1 = prover_channel_1.ReceiveNumber(Pow2(24));

  ProverChannel prover_channel_2(this->channel_prng.Clone());
  uint64_t pow_value_2 = prover_channel_2.ReceiveNumber(Pow2(24));

  EXPECT_EQ(pow_value_1, pow_value_2);

  // Verify.
  VerifierChannel verifier_channel(this->channel_prng.Clone(), prover_channel_1.GetProof());

  verifier_channel.ApplyProofOfWork(0);
  uint64_t pow_value_3 = verifier_channel.GetAndSendRandomNumber(Pow2(24));
  EXPECT_EQ(pow_value_1, pow_value_3);
}

TEST_F(ChannelTest, SendingConsistentWithReceivingRandomBytes) {
  ProverChannel prover_channel(this->channel_prng.Clone());
  std::vector<std::vector<std::byte>> bytes_sent{};

  for (size_t i = 0; i < 100; ++i) {
    auto random_num = this->prng.template UniformInt<uint64_t>(0, 128);
    std::vector<std::byte> bytes_to_send = this->prng.RandomByteVector(random_num);
    prover_channel.SendBytes(bytes_to_send);
    bytes_sent.push_back(bytes_to_send);
  }

  VerifierChannel verifier_channel(this->channel_prng.Clone(), prover_channel.GetProof());
  for (const std::vector<std::byte>& bytes : bytes_sent) {
    ASSERT_EQ(verifier_channel.ReceiveBytes(bytes.size()), bytes);
  }
}

TEST_F(ChannelTest, RandomDataConsistency) {
  ProverChannel prover_channel(this->channel_prng.Clone());

  uint64_t prover_number = prover_channel.ReceiveNumber(1000);
  ExtensionFieldElement prover_test_field_element = prover_channel.ReceiveFieldElement();
  std::vector<std::byte> proof(prover_channel.GetProof());

  VerifierChannel verifier_channel(this->channel_prng.Clone(), proof);
  uint64_t verifier_number = verifier_channel.GetAndSendRandomNumber(1000);
  EXPECT_EQ(verifier_number, prover_number);
  ExtensionFieldElement verifier_test_field_element =
      verifier_channel.GetAndSendRandomFieldElement();
  EXPECT_EQ(verifier_test_field_element, prover_test_field_element);
}

TEST_F(ChannelTest, SendReceiveConsistency) {
  ProverChannel prover_channel(this->channel_prng.Clone());
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());

  auto prover_elem = BaseFieldElement::RandomElement(&prng);
  Blake2s256 prover_commitment = prng.RandomHash();

  prover_channel.SendFieldElement(prover_elem);
  prover_channel.SendCommitmentHash(prover_commitment);

  std::vector<std::byte> proof(prover_channel.GetProof());

  VerifierChannel verifier_channel(this->channel_prng.Clone(), proof);

  EXPECT_FALSE(verifier_channel.IsEndOfProof());
  BaseFieldElement verifier_elem = verifier_channel.ReceiveFieldElement<BaseFieldElement>();
  EXPECT_FALSE(verifier_channel.IsEndOfProof());
  Blake2s256 verifier_commitment = verifier_channel.ReceiveCommitmentHash();
  EXPECT_TRUE(verifier_channel.IsEndOfProof());

  EXPECT_EQ(verifier_elem, prover_elem);
  EXPECT_EQ(verifier_commitment, prover_commitment);

  // Attempt to read beyond the end of the proof.
  EXPECT_ASSERT(verifier_channel.ReceiveCommitmentHash(), HasSubstr("Proof too short."));
}

/*
  This test mimics the expected behavior of a FRI prover while using the channel.
  This is done without integration with the FRI implementation but rather as a complement to
  the FRI test, using a channel mock.
  Semantics of the information sent and received are hence merely a behavioral approximation of
  what will take place in a real scenario. Nevertheless, this test is expected to cover the
  typical usage flow of a STARK/FRI proof protocol.
*/
TEST_F(ChannelTest, FriFlowSimulation) {
  ProverChannel prover_channel(this->channel_prng.Clone());

  Blake2s256 prover_commitment1 = this->prng.RandomHash();
  prover_channel.SendCommitmentHash(prover_commitment1, "First FRI layer");

  ExtensionFieldElement prover_test_field_element1 =
      prover_channel.ReceiveFieldElement("evaluation point");
  ExtensionFieldElement prover_test_field_element2 =
      prover_channel.ReceiveFieldElement("2nd evaluation point");

  auto prover_expected_last_layer_const = ExtensionFieldElement::RandomElement(&this->prng);

  prover_channel.SendFieldElement(prover_expected_last_layer_const, "expected last layer const");

  uint64_t prover_number1 = prover_channel.ReceiveNumber(8, "query index #1 first layer");
  uint64_t prover_number2 = prover_channel.ReceiveNumber(8, "query index #2 first layer");

  std::vector<Blake2s256> prover_decommitment1;
  for (size_t i = 0; i < 15; ++i) {
    prover_decommitment1.push_back(this->prng.RandomHash());
    prover_channel.SendDecommitmentNode(prover_decommitment1.back(), "FRI layer");
  }

  std::vector<std::byte> proof(prover_channel.GetProof());

  VerifierChannel verifier_channel(this->channel_prng.Clone(), proof);

  Blake2s256 verifier_commitment1 = verifier_channel.ReceiveCommitmentHash("First FRI layer");
  EXPECT_EQ(verifier_commitment1, prover_commitment1);
  ExtensionFieldElement verifier_test_field_element1 =
      verifier_channel.GetAndSendRandomFieldElement("evaluation point");
  EXPECT_EQ(verifier_test_field_element1, prover_test_field_element1);
  ExtensionFieldElement vtest_field_element2 =
      verifier_channel.GetAndSendRandomFieldElement("evaluation point ^ 2");
  EXPECT_EQ(vtest_field_element2, prover_test_field_element2);
  ExtensionFieldElement verifier_expected_last_layer_const =
      verifier_channel.ReceiveFieldElement<ExtensionFieldElement>("expected last layer const");
  EXPECT_EQ(verifier_expected_last_layer_const, prover_expected_last_layer_const);
  uint64_t verifier_number1 =
      verifier_channel.GetAndSendRandomNumber(8, "query index #1 first layer");
  EXPECT_EQ(verifier_number1, prover_number1);
  uint64_t verifier_number2 =
      verifier_channel.GetAndSendRandomNumber(8, "query index #2 first layer");
  EXPECT_EQ(verifier_number2, prover_number2);
  std::vector<Blake2s256> verifier_decommitment1;
  for (size_t i = 0; i < 15; ++i) {
    verifier_decommitment1.push_back(verifier_channel.ReceiveDecommitmentNode("FRI layer"));
  }
  EXPECT_EQ(verifier_decommitment1, prover_decommitment1);

  LOG(INFO) << "\n";
  LOG(INFO) << prover_channel;
  LOG(INFO) << "\n";
  LOG(INFO) << verifier_channel;
}

TEST(Channel, NoExpectedAnnotations) {
  VerifierChannel channel({}, {});

  AnnotationScope scope(&channel, "scope");
  channel.GetAndSendRandomNumber(1, "first");
  channel.GetAndSendRandomNumber(2, "second");
}

TEST(Channel, ExpectedAnnotations) {
  VerifierChannel channel({}, {});

  channel.SetExpectedAnnotations({"V->P: /scope: first: Number(0)\n", "WRONG"});

  AnnotationScope scope(&channel, "scope");
  channel.GetAndSendRandomNumber(1, "first");
  EXPECT_ASSERT(channel.GetAndSendRandomNumber(1, "second"), HasSubstr("Annotation mismatch"));
}

TEST(Channel, ExpectedAnnotationsTooShort) {
  VerifierChannel channel({}, {});

  channel.SetExpectedAnnotations({"V->P: /scope: first: Number(0)\n"});

  AnnotationScope scope(&channel, "scope");
  channel.GetAndSendRandomNumber(1, "first");
  EXPECT_ASSERT(channel.GetAndSendRandomNumber(1, "second"), HasSubstr("too short"));
}

TEST(Channel, IgnoreAnnotations) {
  VerifierChannel channel({}, {});

  AnnotationScope scope(&channel, "scope");
  channel.GetAndSendRandomNumber(1, "first");
  ASSERT_EQ(channel.GetAnnotations().size(), 1U);
  channel.DisableAnnotations();
  channel.GetAndSendRandomNumber(2, "second");
  ASSERT_EQ(channel.GetAnnotations().size(), 1U);
}

TEST_F(ChannelTest, BadInput) {
  const size_t num_bytes = 8;
  ProverChannel prover_channel(this->channel_prng.Clone());
  prover_channel.BeginQueryPhase();
  const std::string prover_err_str = "can't receive randomness after query phase has begun.";
  EXPECT_ASSERT(prover_channel.ReceiveBytes(num_bytes), HasSubstr(prover_err_str));

  const std::string verifier_err_str = "can't send randomness after query phase has begun.";
  std::vector<std::byte> prover_data1 = this->RandomByteVector(num_bytes);
  VerifierChannel verifier_channel(this->channel_prng.Clone(), prover_channel.GetProof());
  verifier_channel.BeginQueryPhase();
  EXPECT_ASSERT(verifier_channel.SendBytes(gsl::make_span(prover_data1).as_span<std::byte>());
                , HasSubstr(verifier_err_str));
}

}  // namespace
}  // namespace starkware
