
########################################################################
# Include swig generation macros
########################################################################
find_package(SWIG)

include(GrSwig)
include(GrPython)

########################################################################
# Setup swig generation
########################################################################
set(GR_SWIG_INCLUDE_DIRS
    $<TARGET_PROPERTY:gnuradio::runtime_swig,INTERFACE_INCLUDE_DIRECTORIES>
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

set(GR_SWIG_TARGET_DEPS gnuradio::runtime_swig)

set(GR_SWIG_LIBRARIES gnuradio-liquidDSP)

set(GR_SWIG_DOC_FILE ${CMAKE_CURRENT_BINARY_DIR}/liquidDSP_swig_doc.i)

GR_SWIG_MAKE(liquidDSP_swig liquidDSP_swig.i)

########################################################################
# Install the swig module
########################################################################
GR_SWIG_INSTALL(TARGETS liquidDSP_swig DESTINATION ${GR_PYTHON_DIR}/liquidDSP)

