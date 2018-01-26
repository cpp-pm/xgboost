cmake_minimum_required(VERSION 3.0)

include("cmake/HunterGate.cmake")

### Hunter snapshot that will be used ###
HunterGate(
  URL "https://github.com/ruslo/hunter/archive/v0.19.236.tar.gz"
  SHA1 "b2e48b9eb3ced3e5f8121451335c4d510d6dbbc9"
  )

project(xgboost VERSION 0.4.6)

# -Wno-narrowing  = Don't warn about type narrowing
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wno-c++11-narrowing COMPILER_SUPPORTS_NO_NARROWING)
if(COMPILER_SUPPORTS_NO_NARROWING)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++11-narrowing -Wno-narrowing")
endif()

### Import sugar for source collection ###
hunter_add_package(sugar)
find_package(sugar CONFIG REQUIRED)

sugar_include(.)

option(SKIP_INSTALL "Skip the package install" OFF)
option(XGBOOST_DO_LEAN "Build lean library for prediction only" OFF)
option(XGBOOST_USE_CEREAL "Use cereal serialization" OFF)
if(XGBOOST_USE_CEREAL)
  option(XGBOOST_USE_HALF "Support half precision floating point storage" ON)
endif()

if(XGBOOST_USE_CEREAL)
  #### std::to_string ####
  try_compile(XGBOOST_HAVE_TO_STRING "${CMAKE_BINARY_DIR}/compile_tests" "${PROJECT_SOURCE_DIR}/cmake/to_string.cpp")
  if(XGBOOST_HAVE_TO_STRING)
    set(XGBOOST_ADD_TO_STRING OFF)
  else()
    set(XGBOOST_ADD_TO_STRING ON)
  endif()
else(XGBOOST_USE_CEREAL)
  # This is never needed when CEREAL extensions are disabled
  set(XGBOOST_ADD_TO_STRING OFF)
endif(XGBOOST_USE_CEREAL)

add_subdirectory(src)
