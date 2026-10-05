cmake_minimum_required(VERSION 4.3)

# CMakeList.txt : CMake project for HydrolixQt1, include source and define
# project specific logic here.
#

# Add source to this project's executable.
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable (HydrolixQt1 ${SOURCES} "HydrolixQt1.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HydrolixQt1 PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
