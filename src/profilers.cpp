// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#include "profilers.hpp"
#include <cstdlib>
#include <future>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>

#ifndef ADAPTYST_SCRIPT_PATH
#define ADAPTYST_SCRIPT_PATH "."
#endif

#define ACCEPT_TIMEOUT 5

namespace adaptyst {
  /**
     Constructs a PerfEvent object corresponding to thread tree
     profiling.

     Thread tree profiling traces all system calls relevant to
     spawning new threads/processes and exiting from them so that
     a thread/process tree can be created for later analysis.
  */
  PerfEvent::PerfEvent() {
    this->name = "<thread_tree>";
  }

  /**
     Constructs a PerfEvent object corresponding to on-CPU/off-CPU
     profiling.

     @param freq                  An on-CPU sampling frequency in Hz.
     @param off_cpu_freq          An off-CPU sampling frequency in Hz.
                                  0 disables off-CPU profiling.
     @param buffer_events         A number of on-CPU events that
                                  should be buffered before sending
                                  them for processing. 1
                                  effectively disables buffering.
     @param buffer_off_cpu_events A number of off-CPU events that
                                  should be buffered before sending
                                  them for processing. 0 leaves
                                  the default adaptive buffering, 1
                                  effectively disables buffering.
  */
  PerfEvent::PerfEvent(int freq,
                       int off_cpu_freq,
                       int buffer_events,
                       int buffer_off_cpu_events) {
    this->name = "<main>";
    this->options.push_back(std::to_string(freq));
    this->options.push_back(std::to_string(off_cpu_freq));
    this->options.push_back(std::to_string(buffer_events));
    this->options.push_back(std::to_string(buffer_off_cpu_events));
  }

  /**
     Constructs a PerfEvent object corresponding to a custom
     Linux "perf" event.

     @param name          The name of a "perf" event as displayed by
                          "perf list".
     @param period        A sampling period. The value of X means
                          "do a sample on every X occurrences of the
                          event".
     @param buffer_events A number of events that should be buffered
                          before sending them for processing. 1
                          effectively disables buffering.
  */
  PerfEvent::PerfEvent(std::string name,
                       int period,
                       int buffer_events) {
    this->name = name;
    this->options.push_back(std::to_string(period));
    this->options.push_back(std::to_string(buffer_events));
  }

  /**
     Constructs a Perf object.

     @param acceptor         The acceptor to use for establishing a connection
                             for exchanging generic messages with the profiler.
     @param buf_size         The buffer size for a connection that the acceptor
                             will accept.
     @param perf_bin_path    The full path to the "perf" executable.
     @param perf_python_path The full path to the directory with "perf" Python scripts
                             (usually ending with "libexec/perf-core/scripts/python/Perf-Trace-Util/lib/Perf/Trace")
     @param perf_event       The PerfEvent object corresponding to a "perf" event
                             to be used in this "perf" instance.
     @param cpu_config       A CPUConfig object describing how CPU cores should
                             be used for profiling.
     @param name             The name of this "perf" instance.
  */
  Perf::Perf(std::unique_ptr<Acceptor> &acceptor,
             unsigned int buf_size,
             fs::path perf_bin_path,
             fs::path perf_python_path,
             PerfEvent &perf_event,
             CPUConfig &cpu_config,
             std::string name,
             CaptureMode capture_mode,
             Filter filter) : Profiler(acceptor, buf_size),
                              cpu_config(cpu_config) {
    this->perf_bin_path = perf_bin_path;
    this->perf_python_path = perf_python_path;
    this->perf_event = perf_event;
    this->name = name;
    this->max_stack = 1024;
    this->capture_mode = capture_mode;
    this->filter = filter;

    this->requirements.push_back(std::make_unique<PerfEventKernelSettingsReq>(this->max_stack));
    this->requirements.push_back(std::make_unique<NUMAMitigationReq>());
  }

  std::string Perf::get_name() {
    return this->name;
  }

  void Perf::start(pid_t pid,
                   ServerConnInstrs &connection_instrs,
                   fs::path result_out,
                   fs::path result_processed,
                   bool capture_immediately) {
    std::string instrs = connection_instrs.get_instructions(this->get_thread_count());

    fs::path stdout, stderr_record, stderr_script;
    std::vector<std::string> argv_record;
    std::vector<std::string> argv_script;

    std::string script_path =
      getenv("ADAPTYST_SCRIPT_DIR") ? getenv("ADAPTYST_SCRIPT_DIR") : ADAPTYST_SCRIPT_PATH;

    if (this->perf_event.name == "<thread_tree>") {
      stdout = result_out / "perf_script_syscall_stdout.log";
      stderr_record = result_out / "perf_record_syscall_stderr.log";
      stderr_script = result_out / "perf_script_syscall_stderr.log";

      argv_record = {this->perf_bin_path.string(), "record", "-o", "-",
                     "--call-graph", "fp", "-k",
                     "CLOCK_MONOTONIC", "--buffer-events", "1", "-e",
                     "syscalls:sys_exit_execve,syscalls:sys_exit_execveat,"
                     "sched:sched_process_fork,sched:sched_process_exit",
                     "--sorted-stream", "--pid=" + std::to_string(pid)};
      argv_script = {this->perf_bin_path.string(), "script", "-i", "-", "-s",
                     script_path + "/adaptyst-syscall-process.py",
                     "--demangle", "--demangle-kernel",
                     "--max-stack=" + std::to_string(this->max_stack)};
    } else if (this->perf_event.name == "<main>") {
      stdout = result_out / "perf_script_main_stdout.log";
      stderr_record = result_out / "perf_record_main_stderr.log";
      stderr_script = result_out / "perf_script_main_stderr.log";

      argv_record = {this->perf_bin_path.string(), "record", "-o", "-",
                     "--call-graph", "fp", "-k",
                     "CLOCK_MONOTONIC", "--sorted-stream", "-e",
                     "task-clock", "-F", this->perf_event.options[0],
                     "--off-cpu", this->perf_event.options[1],
                     "--buffer-events", this->perf_event.options[2],
                     "--buffer-off-cpu-events", this->perf_event.options[3],
                     "--pid=" + std::to_string(pid)};
      argv_script = {this->perf_bin_path.string(), "script", "-i", "-", "-s",
                     script_path + "/adaptyst-process.py",
                     "--demangle", "--demangle-kernel",
                     "--max-stack=" + std::to_string(this->max_stack)};
    } else {
      stdout = result_out / ("perf_script_" + this->perf_event.name + "_stdout.log");
      stderr_record = result_out / ("perf_record_" + this->perf_event.name + "_stderr.log");
      stderr_script = result_out / ("perf_script_" + this->perf_event.name + "_stderr.log");

      argv_record = {this->perf_bin_path.string(), "record", "-o", "-",
                     "--call-graph", "fp", "-k",
                     "CLOCK_MONOTONIC", "--sorted-stream", "-e",
                     this->perf_event.name + "/period=" + this->perf_event.options[0] + "/",
                     "--buffer-events", this->perf_event.options[1],
                     "--pid=" + std::to_string(pid)};
      argv_script = {this->perf_bin_path.string(), "script", "-i", "-", "-s",
                     script_path + "/adaptyst-process.py",
                     "--demangle", "--demangle-kernel",
                     "--max-stack=" + std::to_string(this->max_stack)};
    }

    if (this->capture_mode == KERNEL) {
      argv_record.push_back("--kernel-callchains");
    } else if (this->capture_mode == USER) {
      argv_record.push_back("--user-callchains");
    } else if (this->capture_mode == BOTH) {
      argv_record.push_back("--kernel-callchains");
      argv_record.push_back("--user-callchains");
    }

    this->record_proc = std::make_unique<Process>(argv_record);
    this->record_proc->set_redirect_stderr(stderr_record);

    this->script_proc = std::make_unique<Process>(argv_script);
    this->script_proc->add_env("ADAPTYST_SERV_CONNECT", instrs);

    char *cur_pythonpath = getenv("PYTHONPATH");

    if (cur_pythonpath) {
      this->script_proc->add_env("PYTHONPATH",
                                 this->perf_python_path.string() + ":" +
                                 std::string(cur_pythonpath));
    } else {
      this->script_proc->add_env("PYTHONPATH",
                                 this->perf_python_path.string());
    }

    if (this->acceptor.get() != nullptr) {
      std::string instrs = this->acceptor->get_type() + " " +
                           this->acceptor->get_connection_instructions();
      this->script_proc->add_env("ADAPTYST_CONNECT", instrs);
    }

    this->script_proc->set_redirect_stdout(stdout);
    this->script_proc->set_redirect_stderr(stderr_script);

    this->record_proc->set_redirect_stdout(*(this->script_proc));

    this->script_proc->start(false, this->cpu_config, true, result_processed);
    this->record_proc->start(false, this->cpu_config, true, result_processed);

    this->running = true;

    this->process = std::async([&, this]() {
      this->record_proc->close_stdin();
      int code = this->record_proc->join();

      if (code != 0) {
        int status = waitpid(pid, nullptr, WNOHANG);

        if (status == 0) {
          print("Profiler \"" + this->get_name() + "\" (perf-record) has "
                "returned non-zero exit code " + std::to_string(code) + ". "
                "Terminating the profiled command wrapper.", true, true);
          kill(pid, SIGTERM);
        } else {
          print("Profiler \"" + this->get_name() + "\" (perf-record) "
                "has returned non-zero exit code " + std::to_string(code) + " "
                "and the profiled command "
                "wrapper is no longer running.", true, true);
        }

        std::string hint = "Hint: perf-record wrapper has returned exit "
          "code " + std::to_string(code) + ", suggesting something bad "
          "happened when ";

        switch (code) {
        case Process::ERROR_STDOUT:
          print(hint + "creating stdout log file.", true, true);
          break;

        case Process::ERROR_STDERR:
          print(hint + "creating stderr log file.", true, true);
          break;

        case Process::ERROR_STDOUT_DUP2:
          print(hint + "redirecting stdout to perf-script.", true, true);
          break;

        case Process::ERROR_STDERR_DUP2:
          print(hint + "redirecting stderr to file.", true, true);
          break;
        }

        this->running = false;
        return code;
      }

      code = this->script_proc->join();

      if (code != 0) {
        int status = waitpid(pid, nullptr, WNOHANG);

        if (status == 0) {
          print("Profiler \"" + this->get_name() + "\" (perf-script) "
                "has returned non-zero exit code " + std::to_string(code) + ". "
                "Terminating the profiled command wrapper.", true, true);
          kill(pid, SIGTERM);
        } else {
          print("Profiler \"" + this->get_name() + "\" (perf-script) "
                "has returned non-zero exit code " + std::to_string(code) + " "
                "and the profiled command "
                "wrapper is no longer running.", true, true);
        }

        std::string hint = "Hint: perf-script wrapper has returned exit "
          "code " + std::to_string(code) + ", suggesting something bad "
          "happened when ";

        switch (code) {
        case Process::ERROR_STDOUT:
          print(hint + "creating stdout log file.", true, true);
          break;

        case Process::ERROR_STDERR:
          print(hint + "creating stderr log file.", true, true);
          break;

        case Process::ERROR_STDOUT_DUP2:
          print(hint + "redirecting stdout to file.", true, true);
          break;

        case Process::ERROR_STDERR_DUP2:
          print(hint + "redirecting stderr to file.", true, true);
          break;

        case Process::ERROR_STDIN_DUP2:
          print(hint + "replacing stdin with perf-record pipe output.",
                true, true);
          break;
        }
      }

      this->running = false;
      return code;
    });

    while (true) {
      try {
        this->connection = this->acceptor->accept(this->buf_size, ACCEPT_TIMEOUT);
        break;
      } catch (TimeoutException) {
        if (!this->running) {
          return;
        }
      }
    }

    if (this->filter.mode != NONE) {
      nlohmann::json allowdenylist_json = nlohmann::json::object();

      if (this->filter.mode == ALLOW) {
        allowdenylist_json["type"] = "filter_settings";
        allowdenylist_json["data"] = nlohmann::json::object();
        allowdenylist_json["data"]["type"] = "allow";
        allowdenylist_json["data"]["mark"] = this->filter.mark;
        allowdenylist_json["data"]["conditions"] =
          std::get<std::vector<std::vector<std::string> > >(this->filter.data);
      } else if (this->filter.mode == DENY) {
        allowdenylist_json["type"] = "filter_settings";
        allowdenylist_json["data"] = nlohmann::json::object();
        allowdenylist_json["data"]["type"] = "deny";
        allowdenylist_json["data"]["mark"] = this->filter.mark;
        allowdenylist_json["data"]["conditions"] =
          std::get<std::vector<std::vector<std::string> > >(this->filter.data);
      } else if (this->filter.mode == PYTHON) {
        allowdenylist_json["type"] = "filter_settings";
        allowdenylist_json["data"] = nlohmann::json::object();
        allowdenylist_json["data"]["type"] = "python";
        allowdenylist_json["data"]["mark"] = this->filter.mark;
        allowdenylist_json["data"]["script"] =
          std::get<fs::path>(this->filter.data);
      }

      this->connection->write(allowdenylist_json.dump());
    }

    this->connection->write("<STOP>", true);
  }

  unsigned int Perf::get_thread_count() {
    if (this->perf_event.name == "<thread_tree>") {
      return 1;
    } else {
      return this->cpu_config.get_profiler_thread_count();
    }
  }

  void Perf::resume() {
    // TODO
  }

  void Perf::pause() {
    // TODO
  }

  int Perf::wait() {
    return this->process.get();
  }

  std::vector<std::unique_ptr<Requirement> > &Perf::get_requirements() {
    return this->requirements;
  }
};
