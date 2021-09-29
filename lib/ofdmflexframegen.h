#include <gnuradio/block.h>
#include <gnuradio/attributes.h>

#ifndef LIQUIDDSP_API
#  define LIQUIDDSP_API __GR_ATTR_EXPORT
#endif

namespace gr {
  namespace liquidDSP {

    /*!
     * \brief gr-liquidDSP is a set of blocks the famous Liquid DSP
     * library.  https://liquidsdr.org
     *
     * \ingroup liquidDSP
     */
    class LIQUIDDSP_API ofdmflexframegen : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<ofdmflexframegen> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of
       * liquidDSP::ofdmflexframegen.
       *
       * To avoid accidental use of raw pointers, liquidDSP::ofdmflexframegen's
       * constructor is in a private implementation
       * class.  liquidDSP::ofdmflexframegen::make is the public interface for
       * creating new instances.
       */
      static sptr make(size_t in_item_sz, const char *cmd);

      virtual bool unbuffered () const = 0;
      virtual void set_unbuffered (bool unbuffered) = 0;
    };

  } // namespace liquidDSP
} // namespace gr