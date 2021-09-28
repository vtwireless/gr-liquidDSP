/* -*- c++ -*- */

#ifndef INCLUDED_PIPE_FILTER_H
#define INCLUDED_PIPE_FILTER_H

#include <gnuradio/block.h>
#include "api.h"

namespace gr {
  namespace pipe {

    /*!
     * \brief gr-pipe is a set of blocks for using any program as a
     * source, sink or filter by using standard I/O pipes.
     *
     * \ingroup pipe
     */
    class PIPE_API filter : virtual public gr::block
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
      static sptr make(size_t in_item_sz, size_t out_item_sz, double relative_rate, const char *cmd);

      virtual bool unbuffered () const = 0;
      virtual void set_unbuffered (bool unbuffered) = 0;
    };

  } // namespace pipe
} // namespace gr

#endif /* INCLUDED_PIPE_FILTER_H */
