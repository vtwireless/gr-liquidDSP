/* -*- c++ -*- */

#define LIQUIDDSP_API

%include "gnuradio.i" // the common stuff

//load generated python docstrings
%include "liquidDSP_swig_doc.i"

%{
#include "ofdmflexframegen.h"
#include "ofdmflexframesync.h"
%}

%include "ofdmflexframegen.h"
GR_SWIG_BLOCK_MAGIC2(liquidDSP, ofdmflexframegen);
%include "ofdmflexframesync.h"
GR_SWIG_BLOCK_MAGIC2(liquidDSP, ofdmflexframesync);
