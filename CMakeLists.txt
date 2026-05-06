cmake_minimum_required(VERSION 3.16)

project(dlms_profile
  VERSION 0.1.0
  LANGUAGES C CXX)

option(DLMS_BUILD_TESTS "Build GoogleTest tests" ON)
option(DLMS_BUILD_C_API "Build stable C ABI wrapper" ON)
option(DLMS_USE_SYSTEM_GTEST "Use system-installed GoogleTest" OFF)

add_library(dlms_profile
  src/profile/profile_types.cpp
  src/profile/wrapper_tcp_profile_channel.cpp
  src/profile/wrapper_udp_profile_channel.cpp
  src/profile/profile_skeleton.cpp)

if(NOT TARGET dlms_transport AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dlms-transport/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../dlms-transport" "${CMAKE_CURRENT_BINARY_DIR}/_deps/dlms-transport")
endif()

if(NOT TARGET dlms_wrapper AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dlms-wrapper/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../dlms-wrapper" "${CMAKE_CURRENT_BINARY_DIR}/_deps/dlms-wrapper")
endif()

if(NOT TARGET dlms_llc AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dlms-llc/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../dlms-llc" "${CMAKE_CURRENT_BINARY_DIR}/_deps/dlms-llc")
endif()

if(NOT TARGET dlms_hdlc AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dlms-hdlc/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../dlms-hdlc" "${CMAKE_CURRENT_BINARY_DIR}/_deps/dlms-hdlc")
endif()

target_compile_features(dlms_profile PUBLIC cxx_std_11)

target_include_directories(dlms_profile
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(dlms_profile
  PUBLIC
    dlms_transport
    dlms_wrapper
    dlms_llc
    dlms_hdlc)

set(DLMS_PROFILE_HAS_API ON CACHE INTERNAL
  "dlms-profile exposes the APDU channel API required by root integration tests")

if(DLMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
