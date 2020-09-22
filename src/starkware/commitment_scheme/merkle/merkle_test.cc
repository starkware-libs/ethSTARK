#include <algorithm>
#include <cstddef>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/merkle/merkle.h"
#include "starkware/crypt_tools/blake2s_256.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

/*
  Auxiliary function to generate random hashes for the data given to the tree.
*/
std::vector<Blake2s256> GetRandomData(uint64_t length, Prng* prng) {
  std::vector<Blake2s256> data;
  data.reserve(length);
  for (uint64_t i = 0; i < length; ++i) {
    std::vector<std::byte> digest;
    data.push_back(Blake2s256::InitDigestTo(prng->RandomByteVector(Blake2s256::kDigestNumBytes)));
  }
  return data;
}

// Compute root twice, make sure we're consistent.
TEST(MerkleTreeTest, ComputeRootTwice) {
  Prng prng;
  size_t tree_height = prng.UniformInt(0, 10);
  std::vector<Blake2s256> data = GetRandomData(Pow2(tree_height), &prng);
  MerkleTree tree(data.size());
  tree.AddData(data, 0);
  Blake2s256 root1 = tree.GetRoot(tree_height);
  Blake2s256 root2 = tree.GetRoot(tree_height);
  EXPECT_EQ(root1, root2);
}

// Check that starting computation from different depths gets the same value.
TEST(MerkleTreeTest, GetRootFromDifferentDepths) {
  Prng prng;
  size_t tree_height = prng.UniformInt(1, 10);
  // Just to make things interesting - we don't feed the data in one go, but in two segments.
  std::vector<Blake2s256> data = GetRandomData(Pow2(tree_height - 1), &prng);
  MerkleTree tree(data.size() * 2);
  tree.AddData(data, 0);
  tree.AddData(data, data.size());
  for (size_t i = 0; i < 20; ++i) {
    Blake2s256 root1 = tree.GetRoot(prng.UniformInt<size_t>(1, tree_height));
    Blake2s256 root2 = tree.GetRoot(prng.UniformInt<size_t>(1, tree_height));
    EXPECT_EQ(root1, root2);
  }
}

TEST(MerkleTreeTest, GetRootWithInvalidDepth) {
  const size_t tree_height = Pow2(3);
  MerkleTree tree(tree_height);
  EXPECT_ASSERT(tree.GetRoot(4), testing::HasSubstr("Depth should not exceed tree's height."));
  EXPECT_ASSERT(tree.GetRoot(-1), testing::HasSubstr("Depth should not exceed tree's height."));
}

TEST(MerkleTreeTest, InvalidMerkleConstructorInput) {
  const size_t tree_height = Pow2(3) + 1;
  EXPECT_ASSERT(
      MerkleTree tree(tree_height), testing::HasSubstr("Data length is not a power of 2."));
}

TEST(MerkleTreeTest, InvalidGenerateDecommitmentInput) {
  const std::set<uint64_t> empty_queries;
  const Prng channel_prng;
  ProverChannel prover_channel(channel_prng.Clone());
  MerkleTree tree(Pow2(3));
  EXPECT_ASSERT(
      tree.GenerateDecommitment(empty_queries, &prover_channel),
      testing::HasSubstr("Empty input queries."));
  const std::set<uint64_t> out_of_range_queries = {2, 17};
  EXPECT_ASSERT(
      tree.GenerateDecommitment(out_of_range_queries, &prover_channel),
      testing::HasSubstr("Query out of range."));
}

TEST(MerkleTreeTest, InvalidDataSize) {
  Prng prng;
  size_t tree_size = Pow2(3);
  std::vector<Blake2s256> data1 = GetRandomData(tree_size + 1, &prng);
  MerkleTree tree1(tree_size);
  EXPECT_ASSERT(
      tree1.AddData(data1, 0),
      testing::HasSubstr("exceeds the data length declared at tree construction"));
  std::vector<Blake2s256> data2 = GetRandomData(tree_size, &prng);
  MerkleTree tree2(tree_size);
  EXPECT_ASSERT(
      tree2.AddData(data2, 2),
      testing::HasSubstr("exceeds the data length declared at tree construction"));
}

// Check that different trees get different roots.
TEST(MerkleTreeTest, DifferentRootForDifferentTrees) {
  Prng prng;
  size_t tree_height = prng.UniformInt(0, 10);
  std::vector<Blake2s256> data = GetRandomData(Pow2(tree_height), &prng);
  MerkleTree tree(data.size());
  tree.AddData(data, 0);
  Blake2s256 root1 = tree.GetRoot(0);
  tree.AddData(GetRandomData(1, &prng), 0);
  Blake2s256 root2 = tree.GetRoot(tree_height);
  EXPECT_NE(root1, root2);
}

/*
  Generates random queries to a random tree, and checks that the decommitment passes verification.
*/
TEST(MerkleTreeTest, QueryVerificationPositive) {
  Prng prng;
  uint64_t data_length = Pow2(prng.UniformInt(0, 10));
  std::vector<Blake2s256> data = GetRandomData(data_length, &prng);
  MerkleTree tree(data.size());
  tree.AddData(data, 0);
  Blake2s256 root = tree.GetRoot(0);
  std::set<uint64_t> queries;
  size_t num_queries = prng.UniformInt(static_cast<size_t>(1), std::min<size_t>(10, data_length));
  std::map<uint64_t, Blake2s256> query_data;
  while (queries.size() < num_queries) {
    uint64_t query = prng.UniformInt(static_cast<uint64_t>(0), data_length - 1);
    queries.insert(query);
    query_data[query] = data[query];
  }

  const Prng channel_prng;
  ProverChannel prover_channel(channel_prng.Clone());
  tree.GenerateDecommitment(queries, &prover_channel);
  VerifierChannel verifier_channel(channel_prng.Clone(), prover_channel.GetProof());
  EXPECT_TRUE(MerkleTree::VerifyDecommitment(query_data, data_length, root, &verifier_channel));
}

/*
  Generates random queries to a random tree, gets decommitment, and then tries to verify the
  decommitment with data that differs a bit from the one initially given to the tree. We expect it
  to fail.
*/
TEST(MerkleTreeTest, QueryVerificationNegative) {
  Prng prng;
  uint64_t data_length = Pow2(prng.UniformInt(0, 10));
  std::vector<Blake2s256> data = GetRandomData(data_length, &prng);
  MerkleTree tree(data.size());
  tree.AddData(data, 0);
  Blake2s256 root = tree.GetRoot(0);
  std::set<uint64_t> queries;
  size_t num_queries =
      prng.UniformInt(static_cast<uint64_t>(1), std::min<uint64_t>(10, data_length));
  std::map<uint64_t, Blake2s256> query_data;
  while (queries.size() < num_queries) {
    uint64_t query = prng.UniformInt(static_cast<uint64_t>(0), data_length - 1);
    queries.insert(query);
    query_data[query] = data[query];
  }
  // Change one item in the query data.
  auto it = query_data.begin();
  std::advance(it, prng.template UniformInt<size_t>(0, query_data.size() - 1));
  it->second = GetRandomData(1, &prng)[0];

  const Prng channel_prng;
  ProverChannel prover_channel(channel_prng.Clone());
  tree.GenerateDecommitment(queries, &prover_channel);
  VerifierChannel verifier_channel(channel_prng.Clone(), prover_channel.GetProof());
  EXPECT_FALSE(MerkleTree::VerifyDecommitment(query_data, data_length, root, &verifier_channel));
}

}  // namespace
}  // namespace starkware
