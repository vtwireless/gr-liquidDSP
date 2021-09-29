#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <iostream>

#include <gnuradio/io_signature.h>

#include <liquid.h>

#include "ofdmflexframegen.h"
#include "debug.h"


#define NUM_SUBCARRIERS (64)
#define CP_LEN  (16)
#define TAPER_LEN (4)


// Liquid-DSP docs:
//
// https://liquidsdr.org/doc/ofdmflexframe/
//

extern "C" {
// We grouped together a few sets of liquidDSP modulation schemes and
// Forward Error Correction schemes that we share as a single parameter
// with the CRTS parameter API.
struct mod_error_corr_scheme {

    uint32_t mode; // We use as the parameter value.

    // modulation scheme
    modulation_scheme mod;

    // FEC (Forward Error Correction) scheme
    fec_scheme fec;

    // CRTS parameter name.
    const char *scheme_name;
};
}



namespace gr {
  namespace liquidDSP {

    class frame_impl : public ofdmflexframegen
    {
     private:
      size_t d_in_item_sz;
      size_t d_out_item_sz;
      double d_relative_rate;
      bool   d_unbuffered;

      // Runtime data
      int d_cmd_stdin_pipe[2];
      int d_cmd_stdout_pipe[2];
      FILE *d_cmd_stdin;
      FILE *d_cmd_stdout;
      pid_t d_cmd_pid;

      void create_command_process(const char *cmd);
      void create_pipe(int pipe[2]);
      void set_fd_flags(int fd, long flags);
      void reset_fd_flags(int fd, long flags);
      int read_process_output(uint8_t *out, int nitems);
      int write_process_input(const uint8_t *in, int nitems);



static
constexpr struct mod_error_corr_scheme
  modes[12] = {

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


      ::ofdmflexframegen fg = { 0 };
      unsigned char *subcarrierAlloc = 0;
      // Sizes of stream input to output in this ratio:
      const size_t maxBytesIn = 1024;
      const size_t maxBytesOut =
            (NUM_SUBCARRIERS+CP_LEN)*maxBytesIn*sizeof(std::complex<float>);

      // Liquid-DSP lets us add a uint64_t to every frame we send
      // so we add a counter by using this variable.
      uint64_t frameCount = 0;

      // In this case gain is 10  ???
      //static const float softGain = (powf(10.0F, -12.0F/20.0F));
      const float softGain = (10.0);

      int setMode(uint32_t i) {
    const struct mod_error_corr_scheme *mode = modes + i;

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
    };


     public:
      frame_impl(size_t in_item_sz, const char *cmd);
      ~frame_impl();

      // Where all the action really happens
      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      bool unbuffered () const;
      void set_unbuffered (bool unbuffered);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);

    };

// Liquid-DSP docs:
//
// https://liquidsdr.org/doc/ofdmflexframe/
//


    ofdmflexframegen::sptr
    ofdmflexframegen::make(size_t in_item_sz, const char *cmd)
    {
      return gnuradio::get_initial_sptr
        (new frame_impl(in_item_sz, cmd));
    }

    /*
     * The private constructor
     */
    frame_impl::frame_impl(size_t in_item_sz,
            const char *cmd)
      : gr::block("ofdmflexframegen",
              gr::io_signature::make(1, 1, in_item_sz),
              gr::io_signature::make(1, 1, sizeof(std::complex<float>))),
        d_in_item_sz (in_item_sz),
        d_out_item_sz (sizeof(std::complex<float>)),
        d_relative_rate (1.0)
    {
      ofdmflexframegenprops_s fgprops;
      ofdmflexframegenprops_init_default(&fgprops);
      
      std::cerr << "liquid DSP VERSION: " << liquid_version << std::endl;

      set_relative_rate(d_relative_rate);
      create_command_process(cmd);
    }

    /*
     * Our virtual destructor.
     */
    frame_impl::~frame_impl()
    {
      long ret;
      char buf[PIPE_BUF];
      ssize_t sz;
      int i;
      int pstat;

      // Set file descriptors to blocking, to be sure to consume
      // the remaining output generated by the process.
      reset_fd_flags(d_cmd_stdin_pipe[1], O_NONBLOCK);
      reset_fd_flags(d_cmd_stdout_pipe[0], O_NONBLOCK);

      fclose(d_cmd_stdin);

      i = 0;
      do {
        sz = read(d_cmd_stdout_pipe[0], buf, sizeof (buf));
        if (sz < 0) {
          perror("read()");
          break ;
        }
        i++;
      } while (i < 256 && sz > 0);
      fclose(d_cmd_stdout);

      do {
        ret = waitpid(d_cmd_pid, &pstat, 0);
      } while (ret == -1 && errno == EINTR);

      if (ret == -1) {
        perror("waitpid()");
        return ;
      }

      if (WIFEXITED(pstat))
        std::cerr << "Process exited with code " << WEXITSTATUS(pstat) << std::endl;
      else
        std::cerr << "Abnormal process termination" << std::endl;
    }

    bool
    frame_impl::unbuffered() const
    {
      return d_unbuffered;
    }

    void
    frame_impl::set_unbuffered(bool unbuffered)
    {
      d_unbuffered = unbuffered;
    }

    void
    frame_impl::set_fd_flags(int fd, long flags)
    {
      long ret;

      ret = fcntl(fd, F_GETFL);
      if (ret == -1) {
        perror("fcntl()");
        throw std::runtime_error("fcntl() error");
      }

      ret = fcntl(fd, F_SETFL, ret | flags);
      if (ret == -1) {
        perror("fcntl()");
        throw std::runtime_error("fcntl() error");
      }
    }

    void
    frame_impl::reset_fd_flags(int fd, long flag)
    {
      long ret;

      ret = fcntl(fd, F_GETFL);
      if (ret == -1) {
        perror("fcntl()");
        throw std::runtime_error("fcntl() error");
      }

      ret = fcntl(fd, F_SETFL, ret & ~flag);
      if (ret == -1) {
        perror("fcntl()");
        throw std::runtime_error("fcntl() error");
      }
    }

    void
    frame_impl::create_pipe(int pipefd[2])
    {
      int ret;

      ret = ::pipe(pipefd);
      if (ret != 0) {
        perror("pipe()");
        throw std::runtime_error("pipe() error");
      }
    }

    void
    frame_impl::create_command_process(const char *cmd)
    {
      create_pipe(d_cmd_stdin_pipe);
      create_pipe(d_cmd_stdout_pipe);

      d_cmd_pid = fork();
      if (d_cmd_pid == -1) {
        perror("fork()");
        return ;
      }
      else if (d_cmd_pid == 0) {
        dup2(d_cmd_stdin_pipe[0], STDIN_FILENO);
        close(d_cmd_stdin_pipe[0]);
        close(d_cmd_stdin_pipe[1]);

        dup2(d_cmd_stdout_pipe[1], STDOUT_FILENO);
        close(d_cmd_stdout_pipe[1]);
        close(d_cmd_stdout_pipe[0]);

        execl("/bin/sh", "sh", "-c", cmd, NULL);

        perror("execl()");
        exit(EXIT_FAILURE);
      }
      else {
        close(d_cmd_stdin_pipe[0]);
        set_fd_flags(d_cmd_stdin_pipe[1], O_NONBLOCK);
        fcntl(d_cmd_stdin_pipe[1], F_SETFD, FD_CLOEXEC);

        set_fd_flags(d_cmd_stdout_pipe[0], O_NONBLOCK);
        fcntl(d_cmd_stdout_pipe[0], F_SETFD, FD_CLOEXEC);
        close(d_cmd_stdout_pipe[1]);

        d_cmd_stdin = fdopen(d_cmd_stdin_pipe[1], "w");
        if (d_cmd_stdin == NULL) {
          perror("fdopen()");
          throw std::runtime_error("fdopen() error");
          return ;
        }

        d_cmd_stdout = fdopen(d_cmd_stdout_pipe[0], "r");
        if (d_cmd_stdout == NULL) {
          perror("fdopen()");
          throw std::runtime_error("fdopen() error");
          return ;
        }
      }
    }

    void
    frame_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = (double)(noutput_items) / d_relative_rate;
    }

    int
    frame_impl::read_process_output(uint8_t *out, int nitems)
    {
      size_t ret;

      ret = fread(out, d_out_item_sz, nitems, d_cmd_stdout);
      if (    ret == 0
           && ferror(d_cmd_stdout)
           && errno != EAGAIN
           && errno != EWOULDBLOCK) {
        throw std::runtime_error("fread() error");
        return (-1);
      }

      return (ret);
    }

    int
    frame_impl::write_process_input(const uint8_t *in, int nitems)
    {
      size_t ret;

      ret = fwrite(in, d_in_item_sz, nitems, d_cmd_stdin);
      if (    ret == 0
           && ferror(d_cmd_stdin)
           && errno != EAGAIN
           && errno != EWOULDBLOCK) {
        throw std::runtime_error("fwrite() error");
        return (-1);
      }

      if (d_unbuffered)
        fflush(d_cmd_stdin);

      return (ret);
    }

    int
    frame_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const uint8_t *in = (const uint8_t *) input_items[0];
      int n_in_items = ninput_items[0];
      uint8_t *out = (uint8_t *) output_items[0];
      int n_produced;
      int n_consumed;

      n_produced = read_process_output(out, noutput_items);
      if (n_produced < 0)
        return (n_produced);

      n_consumed = write_process_input(in, n_in_items);
      if (n_consumed < 0)
        return (n_consumed);

      consume_each(n_consumed);

      return (n_produced);
    }

  } /* namespace liquidDSP */
} /* namespace gr */
