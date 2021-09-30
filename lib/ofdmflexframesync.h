#include <gnuradio/block.h>
#include <gnuradio/attributes.h>

#ifndef LIQUIDDSP_API
#  define LIQUIDDSP_API __GR_ATTR_EXPORT
#endif

namespace gr {
  namespace liquidDSP {

    /*!
     * \brief gr-liquidDSP is a set of blocks for the famous Liquid DSP
     * library.  https://liquidsdr.org
     *
     * \ingroup liquidDSP
     */
    class LIQUIDDSP_API ofdmflexframesync : virtual public gr::block
    {
     public:

      /*!
       * \brief Return a shared_ptr to a new instance of
       * liquidDSP::ofdmflexframesync.
       */
      static boost::shared_ptr<ofdmflexframesync>
          make(size_t out_item_sz);
    };

  } // namespace liquidDSP
} // namespace gr
