#include <gnuradio/block.h>
#include <gnuradio/attributes.h>


#ifndef API
#  define API __GR_ATTR_EXPORT
#endif


namespace gr {
  namespace liquidDSP {

    /*!
     * \brief gr-liquidDSP is a set of blocks for the famous Liquid DSP
     * library.  https://liquidsdr.org
     *
     * \ingroup liquidDSP
     */
    class API ofdmflexframegen : virtual public gr::block
    {
     public:

      /*!
       * \brief Return a shared_ptr to a new instance of
       * liquidDSP::ofdmflexframegen.
       *
       */
      static boost::shared_ptr<ofdmflexframegen>
          make(size_t in_item_sz);

      // Set the modulation code scheme
      virtual void set_mcs(int mcs) = 0;
    };

  } // namespace liquidDSP
} // namespace gr
