cmake_minimum_required (VERSION 3.5)
if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER /usr/bin/clang++-6.0)
endif()
if (NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER /usr/bin/clang-6.0)
endif()

project(StarkStack VERSION 0.1.0 LANGUAGES CXX C)
include(CTest)
# Basic setup for gtest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
enable_testing()
include_directories("${CMAKE_SOURCE_DIR}/src")

# Basic setup for gflags
find_package(gflags REQUIRED)

if (NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -Wno-mismatched-tags -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

set(CC_OPTIMIZE "-O3")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CC_OPTIMIZE} -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CC_OPTIMIZE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
