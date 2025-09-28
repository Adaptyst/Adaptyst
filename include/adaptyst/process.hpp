#ifndef ADAPTYST_PROCESS_HPP_
#define ADAPTYST_PROCESS_HPP_

#include "socket.hpp"
#include "os_detect.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>
#include <functional>

#ifdef ADAPTYST_UNIX
#include <sys/wait.h>
#endif

namespace adaptyst {
  namespace fs = std::filesystem;

  /**
     A class describing the configuration of CPU cores for performance analysis.

     Specifically, CPUConfig describes what cores should be used for
     performance analysis, what cores should be used for running
     the workflow, what cores should be used for both, and what cores should
     not be used at all.
  */
  class CPUConfig {
  private:
    bool valid;
    int profiler_thread_count;
#ifdef ADAPTYST_UNIX
    cpu_set_t cpu_analysis_set;
    cpu_set_t cpu_workflow_set;
#endif

  public:
    /**
       Constructs an invalid CPUConfig object. This can be useful
       e.g. when using CPUConfig as a class attribute.
    */
    CPUConfig() {
      this->valid = false;
    }

    /**
       Constructs a CPUConfig object.

       @param mask A CPU mask string, where the i-th character
                   defines the purpose of the i-th core as follows:
                   ' ' means "not used",
                   'p' means "used for performance analysis",
                   'c' means "used for the executed workflow", and
                   'b' means "used for both the executed workflow and performance analysis".
    */
    CPUConfig(std::string mask) {
      this->valid = false;
      this->profiler_thread_count = 0;

#ifdef ADAPTYST_UNIX
      CPU_ZERO(&this->cpu_analysis_set);
      CPU_ZERO(&this->cpu_workflow_set);
#endif

      if (!mask.empty()) {
        this->valid = true;

        for (int i = 0; i < mask.length(); i++) {
          if (mask[i] == 'p') {
            this->profiler_thread_count++;
#ifdef ADAPTYST_UNIX
            CPU_SET(i, &this->cpu_analysis_set);
#endif
          } else if (mask[i] == 'c') {
#ifdef ADAPTYST_UNIX
            CPU_SET(i, &this->cpu_workflow_set);
#endif
          } else if (mask[i] == 'b') {
            this->profiler_thread_count++;
#ifdef ADAPTYST_UNIX
            CPU_SET(i, &this->cpu_analysis_set);
            CPU_SET(i, &this->cpu_workflow_set);
#endif
          } else if (mask[i] != ' ') {
            this->valid = false;
            this->profiler_thread_count = 0;
#ifdef ADAPTYST_UNIX
            CPU_ZERO(&this->cpu_analysis_set);
            CPU_ZERO(&this->cpu_workflow_set);
#endif
            return;
          }
        }
      }
    }

    /**
       Returns whether a CPUConfig object is valid.

       A CPUConfig object can be invalid only if the string mask used
       for its construction is invalid.
    */
    bool is_valid() const {
      return this->valid;
    }

    /**
       Returns the number of performance analysis threads that can be spawned
       based on how many cores are allowed for doing the analysis.
    */
    int get_profiler_thread_count() const {
      return this->profiler_thread_count;
    }

#ifdef ADAPTYST_UNIX
    /**
       Returns the sched_setaffinity-compatible CPU set for doing
       the performance analysis.
    */
    cpu_set_t get_cpu_analysis_set() const {
      return this->cpu_analysis_set;
    }

    /**
       Returns the sched_setaffinity-compatible CPU set for running
       the workflow.
    */
    cpu_set_t get_cpu_workflow_set() const {
      return this->cpu_workflow_set;
    }
#endif
  };

  /**
     This class represents an arbitrary code running in a separate
     program process.
  */
  class Process {
  private:
    std::variant<std::vector<std::string>,
      std::function<int()> > command;
    std::unordered_map<std::string, std::string> env;
    bool stdout_redirect;
    bool stdout_terminal;
    fs::path stdout_path;
    bool stderr_redirect;
    fs::path stderr_path;
    bool notifiable;
    bool writable;
    unsigned int buf_size;
    int exit_code;
#ifdef ADAPTYST_UNIX
    int notify_pipe[2];
    int stdin_pipe[2];
    int stdout_pipe[2];
    int *stdout_fd;
    std::unique_ptr<FileDescriptor> stdout_reader;
    std::unique_ptr<FileDescriptor> stdin_writer;
#endif
    bool started;
    bool completed;
    int id;

    inline void close_fd(int fd) {
      if (fd != -1) {
        close(fd);
      }
    }

    void init(unsigned int buf_size) {
      this->stdout_redirect = false;
      this->stdout_terminal = false;
      this->stderr_redirect = false;
      this->notifiable = false;
      this->started = false;
      this->completed = false;
      this->writable = true;
      this->buf_size = buf_size;

#ifdef ADAPTYST_UNIX
      this->stdout_fd = nullptr;
      this->notify_pipe[0] = -1;
      this->notify_pipe[1] = -1;
      this->stdin_pipe[0] = -1;
      this->stdin_pipe[1] = -1;
      this->stdout_pipe[0] = -1;
      this->stdout_pipe[1] = -1;

      char **cur_existing_env_entry = environ;

      while (*cur_existing_env_entry != nullptr) {
        char *cur_entry = *cur_existing_env_entry;

        int sep_index = -1;
        for (int i = 0; cur_entry[i]; i++) {
          if (cur_entry[i] == '=') {
            sep_index = i;
            break;
          }
        }

        if (sep_index == -1) {
          continue;
        }

        std::string key(cur_entry, sep_index);
        std::string value(cur_entry + sep_index + 1);

        if (this->env.find(key) == this->env.end()) {
          this->env[key] = value;
        }

        cur_existing_env_entry++;
      }
#endif
    }

  public:
    /**
       Error exit code when receiving a notification to start the process
       properly.
    */
    static const int ERROR_START = 200;

    /**
       Error exit code when opening or accessing the file for redirecting
       stdout.
    */
    static const int ERROR_STDOUT = 201;

    /**
       Error exit code when opening or accessing the file for redirecting
       stderr.
    */
    static const int ERROR_STDERR = 202;

    /**
       Error exit code when calling dup2() for redirecting stdout.
    */
    static const int ERROR_STDOUT_DUP2 = 203;

    /**
       Error exit code when calling dup2() for redirecting stderr.
    */
    static const int ERROR_STDERR_DUP2 = 204;

    /**
       Error exit code when setting CPU affinity for the process.
    */
    static const int ERROR_AFFINITY = 205;

    /**
       Error exit code when calling dup2() for redirecting stdin.
    */
    static const int ERROR_STDIN_DUP2 = 206;

    /**
       Error exit code due to failing to find the given executable.
    */
    static const int ERROR_NOT_FOUND = 207;

    /**
       Error exit code due to not having sufficient permissions to execute the
       given command.
    */
    static const int ERROR_NO_ACCESS = 208;

    /**
       Error exit code when setting environment variables.
    */
    static const int ERROR_SETENV = 209;

    /**
       Constructs a Process object.

       @param command  Function returning an exit code to execute in
                       a separate process.
       @param buf_size Internal buffer size in bytes.
    */
    Process(std::function<int()> command,
            unsigned int buf_size = 1024) {
      this->command = command;
      this->init(buf_size);
    }

    /**
       Constructs a Process object.

       @param command  Shell command to execute in a separate process.
       @param buf_size Internal buffer size in bytes.
    */
    Process(std::vector<std::string> &command,
            unsigned int buf_size = 1024) {
      if (command.empty()) {
        throw Process::EmptyCommandException();
      }

      this->command = command;
      this->init(buf_size);
    }

    ~Process() {
      if (this->started) {
#ifdef ADAPTYST_UNIX
        if (this->writable &&
            this->stdin_writer.get() != nullptr) {
          this->stdin_writer->close();
        }

        if (this->notifiable) {
          close_fd(this->notify_pipe[1]);
        }

        waitpid(this->id, nullptr, 0);
#endif
      }
    }

    /**
       Adds an environment variable in form of a key-value pair.

       @param key   Environment variable key.
       @param value Environment variable value.
    */
    void add_env(std::string key, std::string value) {
      this->env[key] = value;
    }

    /**
       Redirects stdout to a specified file.

       @param path Path to a stdout file.
    */
    void set_redirect_stdout(fs::path path) {
      this->stdout_redirect = true;
      this->stdout_path = path;
    }

    /**
       Redirects stdout to the terminal.
    */
    void set_redirect_stdout_to_terminal() {
      this->stdout_redirect = true;
      this->stdout_terminal = true;
    }

    /**
       Redirects stdout to another process.

       @param process Process to pipe stdout to.
    */
    void set_redirect_stdout(Process &process) {
      this->stdout_redirect = true;

#ifdef ADAPTYST_UNIX
      this->stdout_fd = &process.stdin_pipe[1];
      process.writable = false;
#else
      this->stdout_redirect = false;
      throw Process::NotImplementedException();
#endif
    }

    /**
       Redirects stderr to a specified file.

       @param path Path to a stderr file.
    */
    void set_redirect_stderr(fs::path path) {
      this->stderr_redirect = true;
      this->stderr_path = path;
    }

    /**
       Spawns a new process executing a code or command specified in the
       constructor and returns the ID of the process immediately.

       @param wait_for_notify Indicates whether the process should wait
                              for a notification before executing. If true,
                              the notification should be sent via notify().
       @param cpu_config      CPU core configuration for the process.
       @param is_analysis     Whether the process corresponds to performance
                              analysis (this is important in the context of
                              cpu_config).
       @param working_path    Working directory of the process.

       @return ID of the spawned process (PID in case of a Unix-based
               system like Linux).
    */
    int start(bool wait_for_notify, const CPUConfig &cpu_config,
              bool is_profiler, fs::path working_path = fs::current_path()) {
      if (wait_for_notify) {
        this->notifiable = true;
      }

#ifdef ADAPTYST_UNIX
      if (this->stdout_redirect && this->stdout_fd != nullptr &&
          *(this->stdout_fd) == -1) {
        throw Process::StartException();
      }

      std::vector<std::string> env_entries;

      for (auto &entry : this->env) {
        env_entries.push_back(entry.first + "=" + entry.second);
      }

      if (this->notifiable && pipe(this->notify_pipe) == -1) {
        throw Process::StartException();
      }

      if (!this->stdout_redirect) {
        if (pipe(this->stdout_pipe) == -1) {
          if (this->notifiable) {
            close_fd(this->notify_pipe[0]);
            close_fd(this->notify_pipe[1]);
            this->notifiable = false;
          }

          throw Process::StartException();
        }

        this->stdout_reader = std::make_unique<FileDescriptor>(this->stdout_pipe,
                                                               nullptr,
                                                               this->buf_size);
      }

      if (pipe(this->stdin_pipe) == -1) {
        if (this->notifiable) {
          close_fd(this->notify_pipe[0]);
          close_fd(this->notify_pipe[1]);
          this->notifiable = false;
        }

        if (!this->stdout_redirect) {
          close_fd(this->stdout_pipe[0]);
          close_fd(this->stdout_pipe[1]);
        }

        throw Process::StartException();
      }

      if (this->writable) {
        this->stdin_writer = std::make_unique<FileDescriptor>(nullptr,
                                                              this->stdin_pipe,
                                                              this->buf_size);
      }

      pid_t forked = fork();

      if (forked == 0) {
        // This executed in a separate process with everything effectively
        // copied (NOT shared!)

        if (this->notifiable) {
          close_fd(this->notify_pipe[1]);
          char buf;
          int bytes_read = 0;
          int received = ::read(this->notify_pipe[0], &buf, 1);
          close_fd(this->notify_pipe[0]);

          if (received <= 0 || buf != 0x03) {
            std::exit(Process::ERROR_START);
          }
        }

        close_fd(this->stdin_pipe[1]);
        close_fd(this->stdout_pipe[0]);

        fs::current_path(working_path);

        if (this->stderr_redirect) {
          int stderr_fd = creat(this->stderr_path.c_str(),
                                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

          if (stderr_fd == -1) {
            std::exit(Process::ERROR_STDERR);
          }

          if (dup2(stderr_fd, STDERR_FILENO) == -1) {
            std::exit(Process::ERROR_STDERR_DUP2);
          }

          close_fd(stderr_fd);
        }

        if (this->stdout_redirect) {
          if (!this->stdout_terminal) {
            int stdout_fd;

            if (this->stdout_fd == nullptr) {
              stdout_fd = creat(this->stdout_path.c_str(),
                                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

              if (stdout_fd == -1) {
                std::exit(Process::ERROR_STDOUT);
              }
            } else {
              stdout_fd = *(this->stdout_fd);
            }

            if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
              std::exit(Process::ERROR_STDOUT_DUP2);
            }

            close_fd(stdout_fd);
          }
        } else {
          if (dup2(this->stdout_pipe[1], STDOUT_FILENO) == -1) {
            std::exit(Process::ERROR_STDOUT_DUP2);
          }

          close_fd(this->stdout_pipe[1]);
        }

        if (dup2(this->stdin_pipe[0], STDIN_FILENO) == -1) {
          std::exit(Process::ERROR_STDIN_DUP2);
        }

        close_fd(this->stdin_pipe[0]);

        if (this->command.index() == 0) {
          std::vector<std::string> elems = std::get<0>(this->command);
          char *argv[elems.size() + 1];

          for (int i = 0; i < elems.size(); i++) {
            argv[i] = (char *)elems[i].c_str();
          }

          argv[elems.size()] = nullptr;

          char *env[env_entries.size() + 1];

          for (int i = 0; i < env_entries.size(); i++) {
            env[i] = (char *)env_entries[i].c_str();
          }

          env[env_entries.size()] = nullptr;

          if (cpu_config.is_valid()) {
            cpu_set_t affinity = is_profiler ? cpu_config.get_cpu_analysis_set() :
              cpu_config.get_cpu_workflow_set();

            if (sched_setaffinity(0, sizeof(affinity), &affinity) == -1) {
              std::exit(Process::ERROR_AFFINITY);
            }
          }

          execvpe(elems[0].c_str(), argv, env);

          // This is reached only if execvpe fails
          switch (errno) {
          case ENOENT:
            std::exit(Process::ERROR_NOT_FOUND);

          case EACCES:
            std::exit(Process::ERROR_NO_ACCESS);

          default:
            std::exit(errno);
          }
        } else {
          std::function<int()> func = std::get<1>(this->command);

          for (auto &entry : this->env) {
            if (setenv(entry.first.c_str(), entry.second.c_str(), 1) == -1) {
              std::exit(Process::ERROR_SETENV);
            }
          }

          std::exit(func());
        }
      }

      if (this->notifiable) {
        close_fd(this->notify_pipe[0]);
      }

      close_fd(this->stdin_pipe[0]);

      if (this->stdout_redirect && this->stdout_fd != nullptr) {
        close_fd(*(this->stdout_fd));
      } else if (!this->stdout_redirect) {
        close_fd(this->stdout_pipe[1]);
      }

      if (forked == -1) {
        if (this->notifiable) {
          close_fd(this->notify_pipe[1]);
          this->notifiable = false;
        }

        throw Process::StartException();
      }

      this->started = true;
      this->id = forked;
      return forked;
#else
      this->notifiable = false;
      throw Process::NotImplementedException();
#endif
    }

    /**
       Spawns a new process executing a code or command specified in the
       constructor and returns the ID of the process immediately,
       with a simplified set of arguments (no notification,
       no specific CPU core configuration, no indication of the process
       as a performance-analysis-related one).

       @param working_path Working directory of the process.

       @return ID of the spawned process (PID in case of a Unix-based
               system like Linux).
    */
    int start(fs::path working_path = fs::current_path()) {
      return start(false, CPUConfig(""), false, working_path);
    }

    /**
       Notifies the process that it can start. Relevant when
       start() has been called with wait_for_notify set to true.
    */
    void notify() {
      if (this->started) {
        if (this->notifiable) {
#ifdef ADAPTYST_UNIX
          FileDescriptor notify_writer(nullptr, this->notify_pipe,
                                       this->buf_size);
          char to_send = 0x03;
          notify_writer.write(1, &to_send);
          this->notifiable = false;
#else
          throw Process::NotImplementedException();
#endif
        } else {
          throw Process::NotNotifiableException();
        }
      } else {
        throw Process::NotStartedException();
      }
    }

    /**
       Reads a line from stdout.
    */
    std::string read_line() {
      if (this->stdout_redirect) {
        throw Process::NotReadableException();
      }

#ifdef ADAPTYST_UNIX
      return this->stdout_reader->read();
#else
      throw Process::NotImplementedException();
#endif
    }

    /**
       Writes data to stdin.

       @param buf  Data to write.
       @param size Number of bytes to write.
    */
    void write_stdin(char *buf, unsigned int size) {
      if (this->started) {
        if (this->writable) {
#ifdef ADAPTYST_UNIX
          this->stdin_writer->write(size, buf);
#else
          throw Process::NotImplementedException();
#endif
        } else {
          throw Process::NotWritableException();
        }
      } else {
        throw Process::NotStartedException();
      }
    }

    /**
       Waits for the process to finish executing.

       @return Exit code of the process.
    */
    int join() {
      if (this->started) {
#ifdef ADAPTYST_UNIX
        int status;
        int result = waitpid(this->id, &status, 0);

        if (result != this->id) {
          throw Process::WaitException();
        }

        this->started = false;
        this->notifiable = false;
        this->completed = true;
        this->exit_code = WEXITSTATUS(status);

        return this->exit_code;
#else
        throw Process::NotImplementedException();
#endif
      } else if (this->completed) {
        return this->exit_code;
      } else {
        throw Process::NotStartedException();
      }
    }

    /**
       Returns whether the process is currently running.

       @return Whether the process is running.
    */
    bool is_running() {
      if (!this->started) {
        return false;
      }

#ifdef ADAPTYST_UNIX
      return waitpid(this->id, nullptr, WNOHANG) == 0;
#else
      throw Process::NotImplementedException();
#endif
    }

    /**
       Closes stdin for writing. This is equivalent
       to sending EOF to stdin.
    */
    void close_stdin() {
      if (!this->writable) {
        throw Process::NotWritableException();
      }

#ifdef ADAPTYST_UNIX
      this->stdin_writer->close();
      this->writable = false;
#else
      throw Process:NotImplementedException();
#endif
    }

    /**
       Terminates the process.
    */
    void terminate() {
#ifdef ADAPTYST_UNIX
      kill(this->id, SIGTERM);
#else
      throw Process::NotImplementedException();
#endif
    }

    /**
       Exception thrown when attempting to read from stdout
       which has been redirected.
    */
    class NotReadableException : public std::exception { };

    /**
       Exception thrown when stdin is not writable.
    */
    class NotWritableException : public std::exception { };

    /**
       Exception thrown when an error occurs during the process
       startup. This is due to either pipe creation failures,
       fork errors, or incorrect redirection configurations.
    */
    class StartException : public std::exception { };

    /**
       Exception thrown when a Process object is attempted to be
       constructed with an empty command.
    */
    class EmptyCommandException : public std::exception { };

    /**
       Exception thrown when join() fails.
    */
    class WaitException : public std::exception { };

    /**
       Exception thrown when a process hasn't been started yet.
    */
    class NotStartedException : public std::exception { };

    /**
       Exception thrown when notify() is called while the process
       has been started with wait_for_notify set to false.
    */
    class NotNotifiableException : public std::exception { };

    /**
       Exception thrown when a feature hasn't been implemented
       yet for a given platform or at all.
    */
    class NotImplementedException : public std::exception { };
  };
};

#endif
