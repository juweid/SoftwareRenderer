# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

set (CMAKE_CXX_STANDARD 20)

project ("SoftwareRenderer" C CXX)

# Include sub-projects.
add_subdirectory("extern")
add_subdirectory("src")
add_subdirectory("examples")