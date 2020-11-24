add_library(verifier_main_helper verifier_main_helper.cc)
target_link_libraries(verifier_main_helper stark channel json proof_system)

add_library(prover_main_helper prover_main_helper.cc)
target_link_libraries(prover_main_helper stark channel json)

add_subdirectory(rescue)
add_subdirectory(ziggy)
