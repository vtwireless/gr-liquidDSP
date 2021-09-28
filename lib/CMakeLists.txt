
########################################################################
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX
list(APPEND pipe_sources
    filter_impl.cc )

set(pipe_sources "${pipe_sources}" PARENT_SCOPE)

add_library(gnuradio-pipe SHARED ${pipe_sources})
target_link_libraries(gnuradio-pipe gnuradio::gnuradio-runtime)

target_include_directories(gnuradio-pipe
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../lib>
    PUBLIC $<INSTALL_INTERFACE:include>
  )

set_target_properties(gnuradio-pipe PROPERTIES DEFINE_SYMBOL "gnuradio_pipe_EXPORTS")


########################################################################
# Install built library files
########################################################################
include(GrMiscUtils)
GR_LIBRARY_FOO(gnuradio-pipe)

########################################################################
# Print summary
########################################################################
message(STATUS "Using install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Building for version: ${VERSION} / ${LIBVER}")
