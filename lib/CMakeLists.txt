
########################################################################
# Setup library
########################################################################

add_library(gnuradio-liquidDSP SHARED filter_impl.cc)
target_link_libraries(gnuradio-liquidDSP gnuradio::gnuradio-runtime)


########################################################################
# Install built library files
########################################################################
include(GrMiscUtils)
GR_LIBRARY_FOO(gnuradio-liquidDSP)
