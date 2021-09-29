
########################################################################
# Setup library
########################################################################
find_package(PkgConfig REQUIRED)
pkg_check_modules(liquid-dsp REQUIRED IMPORTED_TARGET liquid-dsp)

add_library(gnuradio-liquidDSP SHARED ofdmflexframegen.cpp debug.c)
target_link_libraries(gnuradio-liquidDSP gnuradio::gnuradio-runtime
    PkgConfig::liquid-dsp)

########################################################################
# Install built library files
########################################################################
include(GrMiscUtils)
GR_LIBRARY_FOO(gnuradio-liquidDSP)
