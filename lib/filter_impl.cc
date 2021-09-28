#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <iostream>

#include <gnuradio/io_signature.h>

#include "filter.h"


namespace gr {
  namespace liquidDSP {

    class filter_impl : public filter
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

     public:
      filter_impl(size_t in_item_sz, const char *cmd);
      ~filter_impl();

      // Where all the action really happens
      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      bool unbuffered () const;
      void set_unbuffered (bool unbuffered);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);

    };

    filter::sptr
    filter::make(size_t in_item_sz, const char *cmd)
    {
      return gnuradio::get_initial_sptr
        (new filter_impl(in_item_sz, cmd));
    }

    /*
     * The private constructor
     */
    filter_impl::filter_impl(size_t in_item_sz, const char *cmd)
      : gr::block("filter",
              gr::io_signature::make(1, 1, in_item_sz),
              gr::io_signature::make(1, 1, sizeof(std::complex<float>))),
        d_in_item_sz (in_item_sz),
        d_out_item_sz (sizeof(std::complex<float>)),
        d_relative_rate (1.0)
    {
      set_relative_rate(d_relative_rate);
      create_command_process(cmd);
    }

    /*
     * Our virtual destructor.
     */
    filter_impl::~filter_impl()
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
    filter_impl::unbuffered() const
    {
      return d_unbuffered;
    }

    void
    filter_impl::set_unbuffered(bool unbuffered)
    {
      d_unbuffered = unbuffered;
    }

    void
    filter_impl::set_fd_flags(int fd, long flags)
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
    filter_impl::reset_fd_flags(int fd, long flag)
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
    filter_impl::create_pipe(int pipefd[2])
    {
      int ret;

      ret = ::pipe(pipefd);
      if (ret != 0) {
        perror("pipe()");
        throw std::runtime_error("pipe() error");
      }
    }

    void
    filter_impl::create_command_process(const char *cmd)
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
    filter_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = (double)(noutput_items) / d_relative_rate;
    }

    int
    filter_impl::read_process_output(uint8_t *out, int nitems)
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
    filter_impl::write_process_input(const uint8_t *in, int nitems)
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
    filter_impl::general_work (int noutput_items,
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
