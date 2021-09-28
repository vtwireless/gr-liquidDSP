#ifndef INCLUDED_PIPE_API_H
#define INCLUDED_PIPE_API_H

#include <gnuradio/attributes.h>

#ifdef gnuradio_pipe_EXPORTS
#define PIPE_API __GR_ATTR_EXPORT
#else
#define PIPE_API __GR_ATTR_IMPORT
#endif

#endif /* INCLUDED_PIPE_API_H */
