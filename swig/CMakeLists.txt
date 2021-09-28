
########################################################################
# Include swig generation macros
########################################################################
find_package(SWIG)
find_package(PythonLibs)
if(NOT SWIG_FOUND OR NOT PYTHONLIBS_FOUND)
    return()
endif()
include(GrSwig)
include(GrPython)

########################################################################
# Setup swig generation
########################################################################
set(GR_SWIG_INCLUDE_DIRS $<TARGET_PROPERTY:gnuradio::runtime_swig,INTERFACE_INCLUDE_DIRECTORIES>)
set(GR_SWIG_TARGET_DEPS gnuradio::runtime_swig)

set(GR_SWIG_LIBRARIES gnuradio-pipe)

set(GR_SWIG_DOC_FILE ${CMAKE_CURRENT_BINARY_DIR}/pipe_swig_doc.i)
set(GR_SWIG_DOC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

GR_SWIG_MAKE(pipe_swig pipe_swig.i)

########################################################################
# Install the build swig module
########################################################################
GR_SWIG_INSTALL(TARGETS pipe_swig DESTINATION ${GR_PYTHON_DIR}/pipe)

########################################################################
# Install swig .i files for development
########################################################################
install(
    FILES
    pipe_swig.i
    ${CMAKE_CURRENT_BINARY_DIR}/pipe_swig_doc.i
    DESTINATION ${GR_INCLUDE_DIR}/pipe/swig
)
