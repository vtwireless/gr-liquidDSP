/* -*- c++ -*- */

#define LIQUIDDSP_API

%include "gnuradio.i" // the common stuff

//load generated python docstrings
%include "liquidDSP_swig_doc.i"

%{
#include "filter.h"
%}

%include "filter.h"
GR_SWIG_BLOCK_MAGIC2(liquidDSP, filter);
