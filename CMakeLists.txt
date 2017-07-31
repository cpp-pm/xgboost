cmake_minimum_required(VERSION 3.0)

include("cmake/HunterGate.cmake")

### Hunter snapshot that will be used ###
HunterGate(
  URL "https://github.com/ruslo/hunter/archive/v0.19.35.tar.gz"
  SHA1 "bacf59b6856e37964c8eb298a57e0f6ef37e56d6"
  )

project(xgboost VERSION 0.4.6)

# -Wno-narrowing  = Don't warn about type narrowing
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wno-c++11-narrowing COMPILER_SUPPORTS_NO_NARROWING)
if(COMPILER_SUPPORTS_NO_NARROWING)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++11-narrowing -Wno-narrowing")
endif()

### Import sugar for source collection ###
hunter_add_package(Sugar)
include("${SUGAR_ROOT}/cmake/Sugar")
include(sugar_include)

sugar_include(.)

option(SKIP_INSTALL "Skip the package install" OFF)
option(XGBOOST_USE_CEREAL "Use cereal serialization" OFF)
option(XGBOOST_USE_HALF "Support half precision floating point storage" ON)
option(XGBOOST_DO_LEAN "Build lean library for evaluation only" OFF)

if(XGBOOST_USE_CEREAL)
#### std::to_string ####
include(CheckCXXSourceCompiles)  
check_cxx_source_compiles(
"
#include <string>
int main(int argc, char *argv[])
{
    auto str = std::to_string(1);
}
" XGBOOST_HAVE_TO_STRING)
endif()

if(XGBOOST_HAVE_TO_STRING)
  set(XGBOOST_ADD_TO_STRING OFF)
else()
  set(XGBOOST_ADD_TO_STRING ON)
endif()

add_subdirectory(src)
