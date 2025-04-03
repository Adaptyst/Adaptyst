// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#ifndef PROFILERS_HPP_
#define PROFILERS_HPP_

#include "profiling.hpp"
#include "process.hpp"
#include "requirements.hpp"
#include "print.hpp"
#include "server/server.hpp"
#include <regex>
#include <filesystem>
#include <future>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

namespace adaptyst {
  namespace fs = std::filesystem;

  /**
     A class describing a Linux "perf" event, used
     by the Perf class.
  */
  class PerfEvent {
  private:
    std::string name;
    std::vector<std::string> options;

  public:
    friend class Perf;

    // For thread tree profiling
    PerfEvent();

    // For main profiling
    PerfEvent(int freq,
              int off_cpu_freq,
              int buffer_events,
              int buffer_off_cpu_events);

    // For custom event profiling
    PerfEvent(std::string name,
              int period,
              int buffer_events);
  };

  /**
     A class describing a Linux "perf" profiler.
  */
  class Perf : public Profiler {
  public:
    enum CaptureMode {
      KERNEL,
      USER,
      BOTH
    };

    enum FilterMode {
      ALLOW,
      DENY,
      PYTHON,
      NONE
    };

    struct Filter {
      FilterMode mode;
      bool mark;
      std::variant<fs::path,
                   std::vector<std::vector<std::string> > > data;
    };

  private:
    fs::path perf_path;
    std::future<int> process;
    PerfEvent perf_event;
    CPUConfig &cpu_config;
    std::string name;
    std::vector<std::unique_ptr<Requirement> > requirements;
    int max_stack;
    std::unique_ptr<Process> record_proc;
    std::unique_ptr<Process> script_proc;
    CaptureMode capture_mode;
    Filter filter;
    bool running;

  public:
    Perf(std::unique_ptr<Acceptor> &acceptor,
         unsigned int buf_size,
         fs::path perf_path,
         PerfEvent &perf_event,
         CPUConfig &cpu_config,
         std::string name,
         CaptureMode capture_mode,
         Filter filter);
    ~Perf() {}
    std::string get_name();
    void start(pid_t pid,
               ServerConnInstrs &connection_instrs,
               fs::path result_out,
               fs::path result_processed,
               bool capture_immediately);
    unsigned int get_thread_count();
    void resume();
    void pause();
    int wait();
    std::vector<std::unique_ptr<Requirement> > &get_requirements();
  };
};

#endif
