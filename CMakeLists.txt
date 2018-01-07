cmake_minimum_required (VERSION 3.2)

include("cmake/HunterGate.cmake")
HunterGate(
  URL "https://github.com/ruslo/hunter/archive/v0.19.221.tar.gz"
  SHA1 "dc138cc9d1b5badf01e62ab0849f73c82e8ffd1d"  
)

project(xgboost VERSION 0.7.0)
include(cmake/Utils.cmake)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
find_package(OpenMP)

msvc_use_static_runtime()

# Options
option(USE_CUDA  "Build with GPU acceleration") 
option(USE_AVX  "Build with AVX instructions. May not produce identical results due to approximate math." OFF) 
option(R_LIB "Build shared library for R package" OFF)

# TODO
#option(USE_NCCL "Build using NCCL for multi-GPU. Also requires USE_CUDA") 

# Hard coding architectures can result in errors like:
# nvcc fatal   : Unsupported gpu architecture 'compute_70'
# set(GPU_COMPUTE_VER 35;50;52;60;61 CACHE STRING
#  "Space separated list of compute versions to be built against")

# Deprecation warning
if(PLUGIN_UPDATER_GPU)
  set(USE_CUDA ON)
  message(WARNING "The option 'PLUGIN_UPDATER_GPU' is deprecated. Set 'USE_CUDA' instead.")
endif()

# Compiler flags
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(OpenMP_CXX_FOUND OR OPENMP_FOUND)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  # Multithreaded compilation
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
else()
  # Correct error for GCC 5 and cuda
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_MWAITXINTRIN_H_INCLUDED -D_FORCE_INLINES")
  # Performance
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops")
endif()

# Convenience routine to apply list of target_compile_definitions()
set(xgboost_defs "")

# AVX
if(USE_AVX)
  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
  endif()
  list(APPEND xgboost_defs XGBOOST_USE_AVX)
endif()

# compiled code customizations for R package
if(R_LIB)
  list(APPEND xgboost_defs
    XGBOOST_STRICT_R_MODE=1
    XGBOOST_CUSTOMIZE_GLOBAL_PRNG=1
    DMLC_LOG_BEFORE_THROW=0  # DMLC: should be passed in config
    DMLC_DISABLE_STDIN=1     # -/-
    DMLC_LOG_CUSTOMIZE=1     # -/-
    RABIT_CUSTOMIZE_MSG_     # RABIT: should be passed in config
    RABIT_STRICT_CXX98_)     # -/-
endif()

file(GLOB_RECURSE SOURCES 
    src/*.cc
    src/*.h
    include/*.h
)

# Only add main function for executable target
list(REMOVE_ITEM SOURCES ${PROJECT_SOURCE_DIR}/src/cli_main.cc)

file(GLOB_RECURSE CUDA_SOURCES
    src/*.cu
    src/*.cuh
)

set(LINK_LIBRARIES "")

# rabit
hunter_add_package(rabit)
find_package(rabit CONFIG REQUIRED)
list(APPEND LINK_LIBRARIES rabit::rabit)

# dmlc-core
hunter_add_package(dmlc-core)
find_package(dmlc CONFIG REQUIRED)
list(APPEND LINK_LIBRARIES dmlc::dmlc)

set(xgboost_exports "")

if(USE_CUDA)
  
  find_package(CUDA 8.0 REQUIRED)
  cmake_minimum_required(VERSION 3.5)

  list(APPEND xgboost_defs XGBOOST_USE_CUDA)  

  # Compatibiltiy with CMake's CUDA_SELECT_NVCC_ARCH_FLAGS
  # * All     : all CUDA architectures
  # * Auto    : for the detected CUDA architecture only
  # * Common  : all common CUDA architectures
  set(xgboost_CUDA_VALID_TYPES Auto Common All Default Min)
  set(xgboost_CUDA_TYPE Auto CACHE STRING "Cuda type (Auto|Common|All")
  set_property(CACHE xgboost_CUDA_TYPE PROPERTY STRINGS ${xgboost_CUDA_VALID_TYPES})
  if(NOT xgboost_CUDA_TYPE IN_LIST xgboost_CUDA_VALID_TYPES)
    message(FATAL_ERROR "xgboost_CUDA_TYPE must be one of ${xgboost_CUDA_VALID_TYPES}")
  endif()
  cuda_select_nvcc_arch_flags(GENCODE_FLAGS ${xgboost_CUDA_TYPE})

  # cub
  hunter_add_package(cub)
  find_package(cub CONFIG REQUIRED)
  list(APPEND LINK_LIBRARIES cub::cub)
  
  # Hard coding architectures can result in errors like:
  # nvcc fatal   : Unsupported gpu architecture 'compute_70'
  # if((CUDA_VERSION_MAJOR EQUAL 9) OR (CUDA_VERSION_MAJOR GREATER 9))
  #   message("CUDA 9.0 detected, adding Volta compute capability (7.0).")
  #   set(GPU_COMPUTE_VER "${GPU_COMPUTE_VER};70")
  # endif()
  #set(GENCODE_FLAGS "")
  #format_gencode_flags("${GPU_COMPUTE_VER}" GENCODE_FLAGS)

  set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS};--expt-extended-lambda;--expt-relaxed-constexpr;${GENCODE_FLAGS};-lineinfo;")
  if(NOT MSVC)
    set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS};-Xcompiler -fPIC")
    # Note adding -std=c++11 to CUDA_NVCC_FLAGS can cause duplicate flag errors
    # See: https://gitlab.kitware.com/cmake/cmake/merge_requests/1628
    # nvcc fatal   : redefinition of argument 'std'    
    if(NOT "${CMAKE_CXX_FLAGS}" MATCHES "std=c\\+\\+") 
      list(APPEND CUDA_NVCC_FLAGS "-std=c++11") # if language isn't specified already
    endif()
  endif()

  # TODO:
  # if(USE_NCCL)
  #   hunter_add_package(nccl)
  #   find_package(nccl CONFIG REQUIRED)
  #   list(APPEND LINK_LIBRARIES nccl::nccl)
  #   list(APPEND xgboost_defs XGBOOST_USE_NCCL)
  # endif()  

  # NOTE_1: some generators require >= 1 non CUDA files, so we add a "dummy" file
  cuda_add_library(gpuxgboost ${CUDA_SOURCES} cmake/templates/gpu_null.cc STATIC)
  target_include_directories(gpuxgboost PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>")
  target_link_libraries(gpuxgboost ${LINK_LIBRARIES})
  target_compile_definitions(gpuxgboost PUBLIC ${xgboost_defs})
  
  list(APPEND LINK_LIBRARIES gpuxgboost)
  list(APPEND xgboost_exports gpuxgboost)
  
endif()

# flags and sources for R-package
if(R_LIB)
  file(GLOB_RECURSE R_SOURCES
    R-package/src/*.h
    R-package/src/*.c
    R-package/src/*.cc
  )
  list(APPEND SOURCES ${R_SOURCES})
endif()

# Utility objxgboost used in
# * xgboost   : R configuration or standard
# * xgboost4j : JVM_BINDINGS == ON
add_library(objxgboost OBJECT ${SOURCES})
target_include_directories(gpuxgboost PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")  
foreach(dep ${LINK_LIBRARIES})
  target_include_directories(objxgboost PUBLIC $<TARGET_PROPERTY:${dep},INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(objxgboost PUBLIC $<TARGET_PROPERTY:${dep},INTERFACE_COMPILE_DEFINITIONS>)
endforeach()

# building shared library for R package
if(R_LIB)
  find_package(LibR REQUIRED)
  list(APPEND LINK_LIBRARIES "${LIBR_CORE_LIBRARY}")
  MESSAGE(STATUS "LIBR_CORE_LIBRARY " ${LIBR_CORE_LIBRARY})

  get_cmake_property(_variableNames VARIABLES)
  foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
  endforeach()
  
  # Shared library target for the R package:
  # NOTE_2: some generators require >= 1 source files, so we add a "dummy" file
  # we use a unique filename for each library to avoid potential name collisions
  # related to archive post-processing.
  add_library(xgboost SHARED cmake/templates/null.cc $<TARGET_OBJECTS:objxgboost>)
  target_link_libraries(xgboost ${LINK_LIBRARIES})
  foreach(target objxgboost xgboost)
    target_include_directories(${target} PUBLIC "$<BUILD_INTERFACE:${LIBR_INCLUDE_DIRS}>")
  endforeach()  
  
  # R uses no lib prefix in shared library names of its packages
  set_target_properties(xgboost PROPERTIES PREFIX "")

  setup_rpackage_install_target(xgboost ${CMAKE_CURRENT_BINARY_DIR})
  
  # use a dummy location for any other remaining installs
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dummy_inst")

else()

  option(XGBOOST_BUILD_EXAMPLE "Build example" OFF)
  option(JVM_BINDINGS "Build JVM bindings" OFF)    
  option(GOOGLE_TEST "Build google tests" OFF)

  # Library
  # See NOTE_2
  add_library(xgboost cmake/templates/null.cc $<TARGET_OBJECTS:objxgboost>)
  target_link_libraries(xgboost ${LINK_LIBRARIES})
  if(MINGW)
    # remove the 'lib' prefix to conform to windows convention for shared library names
    set_target_properties(xgboost PROPERTIES PREFIX "")
  endif()

  # JVM
  if(JVM_BINDINGS)
    find_package(JNI REQUIRED)
    add_library(xgboost4j SHARED
      $<TARGET_OBJECTS:objxgboost>
      jvm-packages/xgboost4j/src/native/xgboost4j.cpp)
    target_link_libraries(xgboost4j ${LINK_LIBRARIES} ${JAVA_JVM_LIBRARY})
    target_include_directories(xgboost4j PUBLIC
      "$<BUILD_INTERFACE:${JNI_INCLUDE_DIRS}>"
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/jvm-packages/xgboost4j/src/native>")
    target_compile_definitions(xgboost4j PUBLIC ${xgboost_defs})
    list(APPEND xgboost_exports xgboost4j)
  endif()  

  # Executable
  if(XGBOOST_BUILD_EXAMPLE)
    add_executable(runxgboost $<TARGET_OBJECTS:objxgboost> src/cli_main.cc)
    target_link_libraries(runxgboost xgboost)
    target_compile_definitions(testxgboost PUBLIC ${xgboost_defs})
  endif()

  # Test
  if(GOOGLE_TEST)
    #find_package(GTest REQUIRED)
    hunter_add_package(GTest)
    find_package(GTest CONFIG REQUIRED)  
    enable_testing()

    file(GLOB_RECURSE TEST_SOURCES "tests/cpp/*.cc")
    auto_source_group("${TEST_SOURCES}")

    if(USE_CUDA)
      file(GLOB_RECURSE CUDA_TEST_SOURCES "tests/cpp/*.cu")
      set_source_files_properties(${CUDA_TEST_SOURCES} PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ)    
      cuda_add_executable(testxgboost ${TEST_SOURCES} ${CUDA_TEST_SOURCES})
    else()
      add_executable(testxgboost ${TEST_SOURCES})
    endif()
    target_compile_definitions(testxgboost PUBLIC ${xgboost_defs})
    target_include_directories(testxgboost PUBLIC src/tree)    
    target_link_libraries(testxgboost GTest::main xgboost)
    add_test(TestXGBoost testxgboost)
  endif()
  
endif()

foreach(target objxgboost xgboost)
  target_include_directories(${target} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>" #include "src/common/random.h"
    )
  target_compile_definitions(${target} PUBLIC ${xgboost_defs})
endforeach()

list(APPEND xgboost_exports xgboost)

# Group sources
auto_source_group("${SOURCES}")

# Installation (https://github.com/forexample/package-example) {

# Layout. This works for all platforms:
#   * <prefix>/lib/cmake/<PROJECT-NAME>
#   * <prefix>/lib/
#   * <prefix>/include/
set(config_install_dir "lib/cmake/${PROJECT_NAME}")
set(include_install_dir "include")

set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Configuration
set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

# Include module with fuction 'write_basic_package_version_file'
include(CMakePackageConfigHelpers)

# Configure '<PROJECT-NAME>ConfigVersion.cmake'
# Use:
#   * PROJECT_VERSION
write_basic_package_version_file(
    "${version_config}" COMPATIBILITY SameMajorVersion
)

# Configure '<PROJECT-NAME>Config.cmake'
# Use variables:
#   * TARGETS_EXPORT_NAME
#   * PROJECT_NAME
configure_package_config_file(
    "cmake/Config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

# Targets:
#   * <prefix>/lib/libxgboost.a
#   * header location after install: <prefix>/include/xgboost/xgboost.h
#   * headers can be included by C++ code `#include <xgboost/xgboost.h>`

message("XGBOOST_EXPORTS : ${xgboost_exports}")
install(
    TARGETS ${xgboost_exports}
    EXPORT "${TARGETS_EXPORT_NAME}"
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
    RUNTIME DESTINATION "bin"
    INCLUDES DESTINATION "${include_install_dir}"
)

# Headers:
install(
    DIRECTORY "include/"
    DESTINATION "${include_install_dir}"
    FILES_MATCHING PATTERN "*.h"
)

# Config
#   * <prefix>/lib/cmake/xgboost/xgboostConfig.cmake
#   * <prefix>/lib/cmake/xgboost/xgboostConfigVersion.cmake
install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

# Config
#   * <prefix>/lib/cmake/xgboost/xgboostTargets.cmake
install(
    EXPORT "${TARGETS_EXPORT_NAME}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)
# }
