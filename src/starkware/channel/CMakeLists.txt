add_library(channel channel.cc prover_channel.cc prover_channel.cc verifier_channel.cc verifier_channel.cc channel_utils.cc)
target_link_libraries(channel proof_of_work algebra prng profiling third_party)

add_library(proof_of_work proof_of_work.cc)
target_link_libraries(proof_of_work channel profiling)

add_executable(channel_test channel_test.cc prover_channel_test.cc annotation_scope_test.cc)
target_link_libraries(channel_test channel proof_of_work starkware_gtest)
add_test(channel_test channel_test)

add_executable(proof_of_work_test proof_of_work_test.cc)
target_link_libraries(proof_of_work_test proof_of_work  channel profiling starkware_gtest)
add_test(proof_of_work_test channel_test)



