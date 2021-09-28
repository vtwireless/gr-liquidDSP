#include <gnuradio/block.h>
#include <gnuradio/attributes.h>

#ifndef LIQUIDDSP_API
#  define LIQUIDDSP_API __GR_ATTR_EXPORT
#endif

namespace gr {
  namespace liquidDSP {

    /*!
     * \brief gr-pipe is a set of blocks for using any program as a
     * source, sink or filter by using standard I/O pipes.
     *
     * \ingroup pipe
     */
    class LIQUIDDSP_API filter : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<filter> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of pipe::filter.
       *
       * To avoid accidental use of raw pointers, pipe::filter's
       * constructor is in a private implementation
       * class. pipe::filter::make is the public interface for
       * creating new instances.
       */
      static sptr make(size_t in_item_sz, const char *cmd);

      virtual bool unbuffered () const = 0;
      virtual void set_unbuffered (bool unbuffered) = 0;
    };

  } // namespace liquidDSP
} // namespace gr

