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

static
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

        uint8_t *outBuffer;

        //gr::thread::mutex d_mutex;

        unsigned char *subcarrierAlloc = 0;
        ::ofdmflexframesync fs = 0;

        static const int maxBytesOut = 128;
        static const int maxBytesIn = (NUM_SUBCARRIERS+CP_LEN)*maxBytesOut*
                sizeof(std::complex<float>);

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

    set_relative_rate(0.005);


    subcarrierAlloc = (unsigned char *) malloc(NUM_SUBCARRIERS);
    ASSERT(subcarrierAlloc, "malloc() failed");
    ofdmframe_init_default_sctype(NUM_SUBCARRIERS, subcarrierAlloc);

    fs = ofdmflexframesync_create(NUM_SUBCARRIERS, CP_LEN,
            TAPER_LEN, subcarrierAlloc,
            (framesync_callback) frameSyncCallback,
            this/*callback data passed to frameSyncCallback()*/);
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

    ninput_items_required[0] = 1024;
}


int sync_impl::general_work(int noutput_items,
        gr_vector_int &ninput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items) {


    outBuffer = (uint8_t *) output_items[0];

    ofdmflexframesync_execute(fs,
            (std::complex<float> *) input_items[0],
            ninput_items[0]);

    consume_each(ninput_items[0]);

    ASSERT(bytesOut/d_out_item_sz <= noutput_items);

    return bytesOut/d_out_item_sz;
}




extern "C" {

static
int
frameSyncCallback(unsigned char *header, int header_valid,
                unsigned char *payload, unsigned int payload_len,
                int payload_valid, ::framesyncstats_s stats,
                sync_impl *sync) {


    // Initialize for each call.  The bytes out for each call.
    sync->bytesOut = 0;


    if(payload_len <= 0 || !payload_valid) return 0;

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
        memcpy(sync->leftOverBytes + sync->numLeftOverBytes,
                payload, payload_len);
        sync->numLeftOverBytes += payload_len;
        return 0;
    }

    if(sync->numLeftOverBytes) {
        //
        // Write older saved bytes to the output.  The "if" above shows that
        // now we have enough data to write some output.  First write this
        // old data.
        //
        //DSPEW();
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
        //DSPEW("sync->numLeftOverBytes=%d", sync->numLeftOverBytes);

    } else {
        // We no longer have left over bytes.  Let it be known for the
        // next call.
        sync->numLeftOverBytes = 0;
        //DSPEW("sync->numLeftOverBytes=0");
    }


    memcpy(sync->outBuffer, payload, payload_len);

    sync->bytesOut += payload_len;

    // This must be true.  We only write out a multiple of the output
    // type.  That's what all this bullshit code above was for.
    DASSERT(sync->bytesOut % sync->d_out_item_sz == 0);

    return 0;
}
} // extern "c" {



} /* namespace liquidDSP */
} /* namespace gr */

