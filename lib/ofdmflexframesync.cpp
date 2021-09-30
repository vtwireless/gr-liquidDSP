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

#include "ofdmflexframesync.h"
#include "debug.h"
#include "common.h"



namespace gr {
namespace liquidDSP {

extern "C" {

class sync_impl;

int
frameSyncCallback(unsigned char *header, int header_valid,
                unsigned char *payload, unsigned int payload_len,
                int payload_valid, ::framesyncstats_s stats,
                sync_impl *sync);
}



class sync_impl : public ofdmflexframesync {

    private:
        int d_in_item_sz;
        int d_out_item_sz;
        int bytesOut;
        int numLeftOverBytes;
        uint8_t leftOverBytes[32];

        unsigned char *outBuffer;

        //gr::thread::mutex d_mutex;

        unsigned char *subcarrierAlloc = 0;
        ::ofdmflexframesync fs = 0;

        // Sizes of stream input to output in this ratio:
        static const int maxBytesOut = 116;
        static const int maxBytesIn = (NUM_SUBCARRIERS+CP_LEN)*maxBytesOut*
                sizeof(std::complex<float>);

        static double relative_rate;

    public:

        sync_impl(size_t in_item_sz);
        ~sync_impl();

        // This needs to be a C function that is in effect part of this object.
        friend int frameSyncCallback(unsigned char *header, int header_valid,
                unsigned char *payload, unsigned int payload_len,
                int payload_valid, framesyncstats_s stats,
                sync_impl *sync);


        void forecast (int noutput_items, gr_vector_int &ninput_items_required);

        int general_work(int noutput_items,
                gr_vector_int &ninput_items,
                gr_vector_const_void_star &input_items,
                gr_vector_void_star &output_items);
};


boost::shared_ptr<ofdmflexframesync>
ofdmflexframesync::make(size_t out_item_sz) {

    return gnuradio::get_initial_sptr(new sync_impl(out_item_sz));
}


/*
 * The private constructor
 */
sync_impl::sync_impl(size_t out_item_sz)
        : gr::block("ofdmflexframesync",
              gr::io_signature::make(1, 1, sizeof(std::complex<float>)),
              gr::io_signature::make(1, 1, out_item_sz)),
        d_in_item_sz (sizeof(std::complex<float>)),
        d_out_item_sz (out_item_sz),
        numLeftOverBytes (0) {

    std::cerr << "liquid DSP VERSION: " << liquid_version << std::endl;

    ASSERT((d_in_item_sz % d_out_item_sz) == 0);
    ASSERT(d_in_item_sz >= d_out_item_sz);

    relative_rate = ((double) maxBytesOut)/
            ((double) maxBytesIn);

    set_relative_rate(relative_rate);

    subcarrierAlloc = (unsigned char *) malloc(NUM_SUBCARRIERS);
    ASSERT(subcarrierAlloc, "malloc() failed");

    fs = ofdmflexframesync_create(NUM_SUBCARRIERS, CP_LEN,
            TAPER_LEN, subcarrierAlloc,
            (framesync_callback) frameSyncCallback,
            0/*callback data*/);
    ASSERT(fs, "ofdmflexframesync_create() failed");
}


sync_impl::~sync_impl() {

    if(fs) {
        ofdmflexframesync_destroy(fs);
        fs = 0;
    }
    if(subcarrierAlloc) {
        free(subcarrierAlloc);
        subcarrierAlloc = 0;
    }

    std::cerr << "ofdmflexframesync process terminating" << std::endl;
}


void sync_impl::forecast(int noutput_items,
        gr_vector_int &ninput_items_required) {

    ninput_items_required[0] = ((double) noutput_items)/relative_rate;
}


int sync_impl::general_work(int noutput_items,
        gr_vector_int &ninput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items) {

    std::complex<float> *in = (std::complex<float> *) input_items[0];

    int numComplexIn = ninput_items[0];
    if(numComplexIn > maxBytesIn/sizeof(std::complex<float>))
        numComplexIn = maxBytesIn/sizeof(std::complex<float>);

    outBuffer = (unsigned char *) output_items[0];

    ofdmflexframesync_execute(fs, in, numComplexIn);

    consume_each(numComplexIn);

    return bytesOut/d_out_item_sz;
}




extern "C" {

int
frameSyncCallback(unsigned char *header, int header_valid,
                unsigned char *payload, unsigned int payload_len,
                int payload_valid, ::framesyncstats_s stats,
                sync_impl *sync) {


    DSPEW();

    // Initialize for each call.  The bytes out for each call.
    sync->bytesOut = 0;


    if(payload_len <= 0) return 0;

    // In GNUradio stream buffers we can't write partial output types.
    // So, like if the output is floats we can't write 2 bytes, we have to
    // write in units of sizeof(float) which is 4 bytes.  It is possible
    // that we have to store some data in sync->leftOverBytes for a future
    // call to this callback.
    //
    if(payload_len + sync->numLeftOverBytes < sync->d_out_item_sz) {
        //
        // We can't write yet.  We do not have a full size of an output
        // type.  We store the data in sync.leftOverBytes for a later
        // call to this function.
        //
        memcpy(sync->leftOverBytes, payload, payload_len);
        sync->numLeftOverBytes += payload_len;
        return 0;
    }

    if(sync->numLeftOverBytes) {
        //
        // Write older saved bytes to the output.  The "if" above shows that
        // now we have enough data to write some output.  First write this
        // old data.
        //
        memcpy(sync->outBuffer, sync->leftOverBytes, sync->numLeftOverBytes);
        sync->bytesOut += sync->numLeftOverBytes;
        sync->outBuffer += sync->numLeftOverBytes;
    }

    if((payload_len + sync->numLeftOverBytes) % sync->d_out_item_sz) {
        //
        // We need to save some left over bytes for a later call.  We
        // cannot write them now because that would make it not an even
        // number of output types; and GNU radio cannot handle that.
        //
        sync->numLeftOverBytes = (payload_len + sync->numLeftOverBytes) %
            sync->d_out_item_sz;
        uint8_t *from = payload + payload_len - sync->numLeftOverBytes;
        memcpy(sync->leftOverBytes, from, sync->numLeftOverBytes);
        payload_len -= sync->numLeftOverBytes;

    } else
        // We no longer have left over bytes.  Let it be known for the
        // next call.
        sync->numLeftOverBytes = 0;


    memcpy(sync->outBuffer, payload, payload_len);

    sync->bytesOut += payload_len;

    // This must be true.  We only write out a multiple of the output
    // type.  That's what all this bullshit code was for.
    DASSERT(sync->bytesOut % sync->d_out_item_sz == 0);

    return 0;
}
} // extern "c" {



} /* namespace liquidDSP */
} /* namespace gr */

