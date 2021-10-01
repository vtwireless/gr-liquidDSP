#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

#include <iostream>

#include <gnuradio/io_signature.h>

// Liquid-DSP docs:
//
// https://liquidsdr.org/doc/ofdmflexframe/
//
#include <liquid.h>

#include "ofdmflexframegen.h"
#include "debug.h"
#include "common.h"



struct scheme {

    uint32_t mode; // We use as the parameter value.
    // modulation scheme
    modulation_scheme mod;
    // FEC (Forward Error Correction) scheme
    fec_scheme fec;
    const char *scheme_name;
};


static const struct scheme modes[] = {

  {  0, LIQUID_MODEM_BPSK,   LIQUID_FEC_GOLAY2412,  "r1/2 BPSK"       },
  {  1, LIQUID_MODEM_BPSK,   LIQUID_FEC_HAMMING128, "r2/3 BPSK"       },
  {  2, LIQUID_MODEM_QPSK,   LIQUID_FEC_GOLAY2412,  "r1/2 QPSK"       },
  {  3, LIQUID_MODEM_QPSK,   LIQUID_FEC_HAMMING128, "r2/3 QPSK"       },
  {  4, LIQUID_MODEM_QPSK,   LIQUID_FEC_SECDED7264, "r8/9 QPSK"       },
  {  5, LIQUID_MODEM_QAM16,  LIQUID_FEC_HAMMING128, "r2/3 16-QAM"     },
  {  6, LIQUID_MODEM_QAM16,  LIQUID_FEC_SECDED7264, "r8/9 16-QAM"     },
  {  7, LIQUID_MODEM_QAM32,  LIQUID_FEC_SECDED7264, "r8/9 32-QAM"     },
  {  8, LIQUID_MODEM_QAM64,  LIQUID_FEC_SECDED7264, "r8/9 64-QAM"     },
  {  9, LIQUID_MODEM_QAM128, LIQUID_FEC_SECDED7264, "r8/9 128-QAM"    },
  { 10, LIQUID_MODEM_QAM256, LIQUID_FEC_SECDED7264, "r8/9 256-QAM"    },
  { 11, LIQUID_MODEM_QAM256, LIQUID_FEC_NONE,       "uncoded 256-QAM" }
};


namespace gr {
namespace liquidDSP {


class frame_impl : public ofdmflexframegen {

    private:
        int d_in_item_sz;
        int d_out_item_sz;

        gr::thread::mutex d_mutex;

        ::ofdmflexframegen fg = 0;
        unsigned char *subcarrierAlloc = 0;

        // Liquid-DSP lets us add a uint64_t to every frame we send
        // so we add a counter by using this variable.
        uint64_t frameCount = 0;

        // Sizes of stream input to output in this ratio:
        static const int maxBytesIn = 128;
        static const int maxBytesOut = maxBytesIn*
            (NUM_SUBCARRIERS+CP_LEN)*sizeof(std::complex<float>);

        static constexpr double relative_rate = maxBytesOut/maxBytesIn;

        int setMode(uint32_t i);

    public:
    
        frame_impl(size_t in_item_sz);
        ~frame_impl();

        void set_mcs(int mcs);

        void forecast (int noutput_items, gr_vector_int &ninput_items_required);

        int general_work(int noutput_items,
                gr_vector_int &ninput_items,
                gr_vector_const_void_star &input_items,
                gr_vector_void_star &output_items);
};


boost::shared_ptr<ofdmflexframegen>
ofdmflexframegen::make(size_t in_item_sz) {

    return gnuradio::get_initial_sptr(new frame_impl(in_item_sz));
}


void frame_impl::set_mcs(int mcs) {

    if(mcs < 0) mcs = 0;
    else if(mcs > 11) mcs = 11;
    setMode(mcs);
}


int frame_impl::setMode(uint32_t i)
{
    //std::cerr << "liquid DSP VERSION: " << liquid_version << std::endl;
    const struct scheme *mode = modes + i;

    ofdmflexframegenprops_s fgprops; // frame generator properties
    ofdmflexframegenprops_init_default(&fgprops);
    fgprops.check = LIQUID_CRC_32;
    fgprops.fec1 = mode->fec;
    fgprops.fec0 = LIQUID_FEC_NONE;
    // WTF: How come this is a double?
    fgprops.mod_scheme = (double) mode->mod;

    if(!subcarrierAlloc) {
        subcarrierAlloc = (unsigned char *) malloc(NUM_SUBCARRIERS);
        ASSERT(subcarrierAlloc, "malloc() failed");
        ofdmframe_init_default_sctype(NUM_SUBCARRIERS, subcarrierAlloc);
    }

    // Protect fg from general_work()
    gr::thread::scoped_lock guard(d_mutex);

    if(fg) {
        ofdmflexframegen_setprops(fg, &fgprops);
        frameCount = 0;
    } else
        fg = ofdmflexframegen_create(NUM_SUBCARRIERS, CP_LEN,
                TAPER_LEN, subcarrierAlloc, &fgprops);

    ASSERT(fg);

    DSPEW("Set liquid frame scheme to (%" PRIu32
                    "): \"%s\"", mode->mode, mode->scheme_name);

    return 0; // success
}

/*
 * The private constructor
 */
frame_impl::frame_impl(size_t in_item_sz)
        : gr::block("ofdmflexframegen",
              gr::io_signature::make(1, 1, in_item_sz),
              gr::io_signature::make(1, 1, sizeof(std::complex<float>))),
        d_in_item_sz (in_item_sz),
        d_out_item_sz (sizeof(std::complex<float>)) {

    std::cerr << "liquid DSP VERSION: " << liquid_version << std::endl;

    ASSERT((d_out_item_sz % d_in_item_sz) == 0);
    ASSERT(d_out_item_sz >= d_in_item_sz);

    if(setMode(5))
        throw std::runtime_error("ofdmflexframegen failed");

    set_relative_rate(relative_rate);
}


frame_impl::~frame_impl() {

    if(fg) {
        ofdmflexframegen_destroy(fg);
        fg = 0;
        frameCount = 0;
    }

    if(subcarrierAlloc) {
        free(subcarrierAlloc);
        subcarrierAlloc = 0;
    }

    std::cerr << "process terminating" << std::endl;
}


void frame_impl::forecast(int noutput_items,
        gr_vector_int &ninput_items_required) {

    ninput_items_required[0] = ((double) noutput_items)/relative_rate;
}


int frame_impl::general_work(int noutput_items,
        gr_vector_int &ninput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items) {

    // Protect fg from setMode()
    gr::thread::scoped_lock guard(d_mutex);

    std::complex<float> *obuf = (std::complex<float> *) output_items[0];

    int lenIn = ninput_items[0]*d_in_item_sz;

    if(lenIn > maxBytesIn)
        lenIn = maxBytesIn;

    ofdmflexframegen_assemble(
            fg, (unsigned char *) &frameCount,
            (const unsigned char *) input_items[0], lenIn);
    ++frameCount;
 
    bool last_symbol = false;

#define COMPLEX_PER_WRITE  (8)

    int numComplexOut = 0;

    // The interface to ofdmflexframegen_write()
    // https://liquidsdr.org/doc/ofdmflexframe/
    //
    while(!last_symbol) {

        ASSERT(numComplexOut + COMPLEX_PER_WRITE <=
                maxBytesOut/sizeof(std::complex<float>));
        last_symbol = ofdmflexframegen_write(fg, obuf, COMPLEX_PER_WRITE);
        obuf += COMPLEX_PER_WRITE;
        numComplexOut += COMPLEX_PER_WRITE;
    }

    consume_each(lenIn / d_in_item_sz);

    return numComplexOut;
}



} /* namespace liquidDSP */
} /* namespace gr */
