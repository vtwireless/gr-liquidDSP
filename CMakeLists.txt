cmake_minimum_required(VERSION 3.8)
project(gr-liquidDSP CXX C)

set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")


set(VERSION_MAJOR 0)
set(VERSION_API   0)
set(VERSION_ABI   0)


########################################################################
# Compiler specific setup
########################################################################
add_definitions(-fvisibility=hidden)
set(CMAKE_CXX_STANDARD 11)


########################################################################
# Install directories
########################################################################
find_package(Gnuradio "3.8" REQUIRED)
include(GrVersion)


# GNU radio requires these next 3 lines:
include(GrPlatform) #defines LIB_SUFFIX
set(CMAKE_MODULES_DIR lib${LIB_SUFFIX}/cmake)
set(GR_CMAKE_DIR lib${LIB_SUFFIX}/cmake/liquidDSP)


########################################################################
# Add subdirectories
########################################################################
add_subdirectory(lib)
add_subdirectory(swig)
add_subdirectory(python)
add_subdirectory(grc)
