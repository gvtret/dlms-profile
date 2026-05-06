cmake_minimum_required(VERSION 3.16)

project(dlms_profile
  VERSION 0.1.0
  LANGUAGES C CXX)

option(DLMS_BUILD_TESTS "Build GoogleTest tests" ON)
option(DLMS_BUILD_C_API "Build stable C ABI wrapper" ON)
option(DLMS_USE_SYSTEM_GTEST "Use system-installed GoogleTest" OFF)

add_library(dlms_profile
  src/profile/profile_skeleton.cpp)

target_compile_features(dlms_profile PUBLIC cxx_std_11)

target_include_directories(dlms_profile
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(DLMS_PROFILE_HAS_API ON CACHE INTERNAL
  "dlms-profile exposes the APDU channel API required by root integration tests")

if(DLMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

