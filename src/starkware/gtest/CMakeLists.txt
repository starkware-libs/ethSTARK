add_library(starkware_gtest STATIC gtest_main.cc)
target_link_libraries(starkware_gtest prng blake2s starkware_common third_party gmock gtest)
