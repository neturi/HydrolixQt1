# CMakeLists require a minimum CMake version and project declaration.
cmake_minimum_required(VERSION 4.3)
project(HydrolixQt1)

# CMakeList.txt : CMake project for HydrolixQt1, include source and define
# project specific logic here.
#

# Collect all .cpp sources in this directory and build the executable from them.
# This ensures helper/implementation translation units (CommonValuesTests.cpp,
# ComparisonBenchmarks.cpp, etc.) are compiled and linked into the final binary.
## Delegate building the HydrolixQt1 target to the subdirectory
## to avoid duplicate or incorrectly specified target sources here.
add_subdirectory(HydrolixQt1)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HydrolixQt1 PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.

