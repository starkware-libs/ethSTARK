add_executable(fft_test fft_test.cc)
target_link_libraries(fft_test algebra starkware_gtest)
add_test(fft_test fft_test)
