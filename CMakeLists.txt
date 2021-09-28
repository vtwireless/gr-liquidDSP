cmake_minimum_required(VERSION 3.8)
project(gr-pipe CXX)

set(CMAKE_BUILD_TYPE "Debug")



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

include(GrPlatform) #define LIB_SUFFIX

set(CMAKE_MODULES_DIR lib${LIB_SUFFIX}/cmake)

# GR_INCLUDE_DIR is needed by swig
set(GR_INCLUDE_DIR      include/pipe)
set(GR_CMAKE_DIR        lib${LIB_SUFFIX}/cmake/pipe)
set(GR_PKG_LIBEXEC_DIR  ${GR_LIBEXEC_DIR}/${CMAKE_PROJECT_NAME})


########################################################################
# Add subdirectories
########################################################################
add_subdirectory(lib)
add_subdirectory(swig)
add_subdirectory(python)
add_subdirectory(grc)
