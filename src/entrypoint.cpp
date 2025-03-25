// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#include "entrypoint.hpp"
#include "print.hpp"
#include "profiling.hpp"
#include "profilers.hpp"
#include "server/socket.hpp"
#include "cmd.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/predef.h>
#include <regex>
#include <sys/wait.h>

#ifndef ADAPTYST_CONFIG_FILE
#define ADAPTYST_CONFIG_FILE ""
#endif

namespace adaptyst {
  namespace ch = std::chrono;
  using namespace std::chrono_literals;

  bool quiet;

  /**
     A class validating whether a supplied command-line
     option is equal to or larger than a given value.
  */
  class OnlyMinRange : public CLI::Validator {
  public:
    OnlyMinRange(int min) {
      func_ = [=](const std::string &arg) {
        if (!std::regex_match(arg, std::regex("^-?[0-9]+$")) ||
            std::stoi(arg) < min) {
          return "The value must be a number equal to or greater than " +
            std::to_string(min);
        }

        return std::string();
      };
    }
  };

  /**
     Entry point to the Adaptyst frontend when it is run from
     the command line.
  */
  int main_entrypoint(int argc, char **argv) {
    CLI::App app("Adaptyst: a performance analysis tool");
    app.formatter(std::make_shared<PrettyFormatter>());

    bool print_version = false;
    app.add_flag("-v,--version", print_version, "Print version and exit");

    unsigned int freq = 10;
    app.add_option("-F,--freq", freq, "Sampling frequency per second for "
                   "on-CPU time profiling (default: 10)")
      ->check(OnlyMinRange(1))
      ->option_text("UINT>0");

    unsigned int buffer = 1;
    app.add_option("-B,--buffer", buffer, "Buffer up to this number of "
                   "events before sending data for processing "
                   "(1 effectively disables buffering) (default: 1)")
      ->check(OnlyMinRange(1))
      ->option_text("UINT>0");

    int off_cpu_freq = 1000;
    app.add_option("-f,--off-cpu-freq", off_cpu_freq, "Sampling frequency "
                   "per second for off-CPU time profiling "
                   "(0 disables off-CPU profiling, -1 makes Adaptyst "
                   "capture *all* off-CPU events) (default: 1000)")
      ->check(OnlyMinRange(-1))
      ->option_text("UINT or -1");

    unsigned int off_cpu_buffer = 0;
    app.add_option("-b,--off-cpu-buffer", off_cpu_buffer, "Buffer up to "
                   "this number of off-CPU events before sending data "
                   "for processing (0 leaves the default "
                   "adaptive buffering, 1 effectively disables buffering) "
                   "(default: 0)")
      ->check(OnlyMinRange(0))
      ->option_text("UINT");

    unsigned int post_process = 1;
    int max_allowed = std::thread::hardware_concurrency() - 3;

    if (max_allowed < 1) {
      max_allowed = 1;
    }

    app.add_option("-p,--post-process", post_process, "Number of threads "
                   "isolated from profiled command to use for profilers "
                   "and processing (must not be greater than " +
                   std::to_string(max_allowed) + "). Use 0 to not "
                   "isolate profiler and processing threads "
                   "from profiled command threads (NOT RECOMMENDED). "
                   "(default: 1)")
      ->check(CLI::Range(0, max_allowed))
      ->option_text("UINT");

    std::string address = "";
    app.add_option("-a,--address", address, "Delegate processing to "
                   "another machine running adaptyst-server. All results "
                   "will be stored on that machine.")
      ->check([](const std::string &arg) {
        if (!std::regex_match(arg, std::regex("^.+\\:[0-9]+$"))) {
          return "The value must be in form of \"<address>:<port>\".";
        }

        return "";
      })
      ->option_text("ADDRESS:PORT");

    std::string codes_dst = "";
    app.add_option("-c,--codes", codes_dst, "Send the newline-separated list "
                   "of detected source code files to a specified destination "
                   "rather than pack the code files on the same machine where "
                   "a profiled program is run. The value can be either \"srv\" "
                   "(i.e. the server receives the list, looks for the "
                   "files there, and creates a source code archive there as "
                   "well), \"file:<path>\" (i.e. the list is saved to <path> "
                   "and can be then read e.g. by adaptyst-code), or "
                   "\"fd:<number>\" (i.e. the list is written to a specified "
                   "file descriptor).")
      ->check([](const std::string &arg) {
        if (!std::regex_match(arg, std::regex("^(file\\:.+|fd:\\d+|srv)$"))) {
          return "The value must be in form of \"srv\", \"file:<path>\", or "
            "\"fd:<number>\".";
        }

        return "";
      })
      ->option_text("TYPE[:ARG]");

    unsigned int server_buffer = 1024;
    app.add_option("-s,--server-buffer", server_buffer, "Communication "
                   "buffer size in bytes for internal adaptyst-server. "
                   "Not to be used with -a. (default when no -a: 1024)")
      ->check(OnlyMinRange(1))
      ->option_text("UINT>0")
      ->excludes("-a");

    unsigned int warmup = 1;
    app.add_option("-w,--warmup", warmup, "Warmup time in seconds between "
                   "adaptyst-server signalling readiness for receiving "
                   "data and starting the profiled program. Increase this "
                   "value if you see missing information after profiling "
                   "(note that adaptyst-server is also used internally "
                   "if no -a option is specified). (default: 1)")
      ->check(OnlyMinRange(1))
      ->option_text("UINT>0");

    std::vector<std::string> event_strs;
    app.add_option("-e,--event", event_strs, "Extra perf event to be used "
                   "for sampling with a given period (i.e. do a sample on "
                   "every PERIOD occurrences of an event and display the "
                   "results under the title TITLE in a website). Run "
                   "\"perf list\" for the list of possible events. You "
                   "can specify multiple events by specifying this option "
                   "more than once. Use quotes if you need to use spaces.")
      ->check([](const std::string &arg) {
        std::smatch match;

        if (!std::regex_match(arg, match, std::regex("^.+,[0-9\\.]+,(.+)$"))) {
          return "The value \"" + arg + "\" must be in form of EVENT,PERIOD,TITLE "
            "(PERIOD must be a number).";
        }

        if (match[1] == "CARM_SSP" ||
            match[1] == "CARM_SDP" ||
            match[1] == "CARM_SSESP" ||
            match[1] == "CARM_SSEDP" ||
            match[1] == "CARM_AVX2SP" ||
            match[1] == "CARM_AVX2DP" ||
            match[1] == "CARM_AVX512SP" ||
            match[1] == "CARM_AVX512DP" ||
            match[1] == "CARM_MEM_LDST") {
          return "The title in \"" + arg + "\" is a reserved keyword "
            "(" + match[1].str() + "), you cannot use it.";
        }

        return std::string();
      })
      ->option_text("EVENT,PERIOD,TITLE")
      ->take_all();

#ifdef BOOST_ARCH_X86
    unsigned int roofline_freq = 0;
    app.add_option("-r,--roofline", roofline_freq, "Run also "
                   "cache-aware roofline profiling with the specified sampling "
                   "frequency per second")
      ->check(OnlyMinRange(1))
      ->option_text("UINT>0");
#endif

    quiet = false;
    app.add_flag("-q,--quiet", quiet, "Do not print anything (if set, check "
                 "exit code for any errors)");

    bool call_split_unix = true;

    for (int i = 0; i < argc; i++) {
      if (strcmp(argv[i], "--") == 0) {
        call_split_unix = false;
        break;
      }
    }

    std::vector<std::string> command_parts;
    std::vector<std::string> command_elements;
    app.add_option("COMMAND", command_parts, "Command to be profiled (required)")
      ->check([&call_split_unix, &command_elements](const std::string &arg) {
        const char *not_valid = "The command you have provided is not a valid one!";

        if (arg.empty()) {
          return not_valid;
        } else if (call_split_unix) {
          std::vector<std::string> parts = boost::program_options::split_unix(arg);

          if (parts.empty()) {
            return not_valid;
          } else {
            for (auto &part : parts) {
              command_elements.push_back(part);
            }
          }
        } else {
          command_elements.push_back(arg);
        }

        return "";
      })
      ->option_text(" ")
      ->take_all();

    CLI11_PARSE(app, argc, argv);

    if (print_version) {
      std::cout << version << std::endl;
      return 0;
    } else if (codes_dst == "srv" && address == "") {
      std::cerr << "--codes cannot be set to \"srv\" if no -a option is "
        "specified!" << std::endl;
      return 3;
    } else if (command_parts.empty()) {
      std::cerr << "You need to provide the command to be profiled!" << std::endl;
      return 3;
    } else {
      auto start_time =
        ch::duration_cast<ch::milliseconds>(ch::system_clock::now().time_since_epoch()).count();

      print_notice();

      print("Reading config file...", false, false);

      fs::path local_config_path = fs::path(getenv("HOME")) /
        ".adaptyst" / "adaptyst.conf";
      std::unordered_map<std::string, std::string> config;

      auto read_config = [](fs::path config_path,
                            std::unordered_map<std::string, std::string> &result) {
        std::ifstream stream(config_path);

        if (!stream) {
          print("Cannot open or find " + config_path.string() + ", ignoring.",
                true, false);
          return true;
        }

        int cur_line = 1;

        while (stream) {
          std::string line;
          std::getline(stream, line);

          if (line.empty() || line[0] == '#') {
            cur_line++;
            continue;
          }

          std::smatch match;

          if (!std::regex_match(line, match,
                                std::regex("^(\\S+)\\s*\\=\\s*(.+)$"))) {
            print("Syntax error in line " + std::to_string(cur_line) + " of " +
                  config_path.string() + "!", true, true);
            return false;
          }

          result[match[1]] = match[2];
          cur_line++;
        }

        print("Successfully read " + config_path.string() + ".", true, false);
        return true;
      };

      if (!read_config(ADAPTYST_CONFIG_FILE, config) ||
          !read_config(local_config_path, config)) {
        return 2;
      }

      if (config.find("perf_path") == config.end()) {
        print("You must specify the path to your patched \"perf\" installation "
              "(perf_path) in your config file (" + local_config_path.string() +
              " or " + ADAPTYST_CONFIG_FILE + ")!", true, true);
        return 2;
      }

      fs::path perf_path(config["perf_path"]);
      perf_path = perf_path / "bin" / "perf";

      if (!fs::exists(perf_path)) {
        print(perf_path.string() + " does not exist!", true, true);
        print("Hint: You may want to verify perf_path in your config file (" +
              local_config_path.string() + " or " + ADAPTYST_CONFIG_FILE + ").",
              false, true);
        return 2;
      }

      if (!fs::is_regular_file(fs::canonical(perf_path))) {
        print(perf_path.string() + " does not point to regular file!", true, true);
        print("Hint: You may want to verify perf_path in your config file (" +
              local_config_path.string() + " or " + ADAPTYST_CONFIG_FILE + ").",
              false, true);
        return 2;
      }

      print("Creating temporary directory...", false, false);

      pid_t current_pid = getpid();
      fs::path tmp_dir = fs::temp_directory_path() /
        ("adaptyst.pid." + std::to_string(current_pid));

      try {
        if (fs::exists(tmp_dir)) {
          fs::remove_all(tmp_dir);
        }

        fs::create_directories(tmp_dir);
      } catch (fs::filesystem_error) {
        print("Could not create " + tmp_dir.string() + "! Exiting.",
              true, true);
      }

      print("In case of any issues, check the files inside " +
            tmp_dir.string() + ".",
            true, false);

      print("Checking CPU specification...", false, false);

      CPUConfig cpu_config = get_cpu_config(post_process,
                                            address != "");

      if (!cpu_config.is_valid()) {
        return 1;
      }

      cpu_set_t cpu_set = cpu_config.get_cpu_profiler_set();
      sched_setaffinity(0, sizeof(cpu_set), &cpu_set);

      std::vector<std::unique_ptr<Profiler> > profilers;

      PerfEvent main(freq, off_cpu_freq, buffer, off_cpu_buffer);
      PerfEvent syscall_tree;

      profilers.push_back(std::make_unique<Perf>(perf_path, syscall_tree, cpu_config,
                                                 "Thread tree profiler"));
      profilers.push_back(std::make_unique<Perf>(perf_path, main, cpu_config,
                                                 "On-CPU/Off-CPU profiler"));

      std::unique_ptr<fs::path> roofline_benchmark_path;

#ifdef BOOST_ARCH_X86
      if (roofline_freq > 0) {
        print("Setting up roofline profiling...", false, false);

        if (config.find("roofline_benchmark_path") == config.end()) {
          print("No roofline benchmarking results are provided in the config file, "
                "running the CARM tool...(this may take a *long* while, be patient)",
                true, false);
          print("If you already have the results somewhere else, put the path "
                "to them in roofline_benchmark_path in your config file (" +
                local_config_path.string() + " or " + ADAPTYST_CONFIG_FILE + ").",
                true, false);

          if (config.find("carm_tool_path") == config.end()) {
            print("No path to the CARM tool specified! Please download the tool "
                  "from https://github.com/champ-hub/carm-roofline and "
                  "put the path to it in carm_tool_path in your config file (" +
                  local_config_path.string() + " or " + ADAPTYST_CONFIG_FILE +
                  "). See the Adaptyst documentation for more information.",
                  true, true);
            return 2;
          }

          std::vector<std::string> command =
            {"python3", fs::path(config["carm_tool_path"]) / "run.py",
             "-out", tmp_dir};

          Process process(command);
          process.set_redirect_stdout_to_terminal();
          process.start();

          int exit_code = process.join();

          if (exit_code != 0) {
            print("The CARM tool has returned a non-zero exit code " +
                  std::to_string(exit_code) + ". Exiting.", true, true);
            return 2;
          }

          if (fs::copy_file(tmp_dir / "roofline" / "unnamed_roofline.csv",
                            local_config_path.parent_path() / "roofline.csv")) {
            roofline_benchmark_path =
              std::make_unique<fs::path>(local_config_path.parent_path() / "roofline.csv");

            std::ofstream config_output(local_config_path, std::ios_base::app);

            if (!config_output) {
              print("Could not open " + local_config_path.string() + " for writing! " +
                    "Continuing, but you will need to put " +
                    roofline_benchmark_path->string() + " in roofline_benchmark_path "
                    "in your config file manually.", true, false);
            } else {
              config_output << "roofline_benchmark_path=";
              config_output << roofline_benchmark_path->string();
              config_output << std::endl;
            }
          } else {
            print("Could not copy the roofline benchmark results to the Adaptyst local "
                  "config directory! Continuing, but Adaptyst will have to run roofline "
                  "benchmarking again next time.", true, false);
            print("You may want to run the CARM tool manually and update "
                  "roofline_benchmark_path in your config file (" + local_config_path.string() +
                  " or " + ADAPTYST_CONFIG_FILE + "). See the Adaptyst documentation for "
                  "more information.", true, false);

            roofline_benchmark_path =
              std::make_unique<fs::path>(tmp_dir / "roofline" / "unnamed_roofline.csv");
          }
        } else {
          roofline_benchmark_path =
            std::make_unique<fs::path>(config["roofline_benchmark_path"]);

          if (!fs::exists(*roofline_benchmark_path)) {
            print(roofline_benchmark_path->string() + " does not exist!",
                  true, true);
            print("Hint: You may want to verify roofline_benchmark_path "
                  "in your config file (" +
                  local_config_path.string() + " or " + ADAPTYST_CONFIG_FILE + ").",
                  false, true);

            return 2;
          }

          if (!fs::is_regular_file(fs::canonical(*roofline_benchmark_path))) {
            print(roofline_benchmark_path->string() + " does not point to "
                  "a regular file!", true, true);
            print("Hint: You may want to verify roofline_benchmark_path "
                  "in your config file (" +
                  local_config_path.string() + " or " + ADAPTYST_CONFIG_FILE + ").",
                  false, true);

            return 2;
          }
        }

        std::string freq = std::to_string(roofline_freq);
        event_strs.push_back("fp_arith_inst_retired.scalar_single," + freq + ",CARM_SSP");
        event_strs.push_back("fp_arith_inst_retired.scalar_double," + freq + ",CARM_SDP");
        event_strs.push_back("fp_arith_inst_retired.128b_packed_single," + freq + ",CARM_SSESP");
        event_strs.push_back("fp_arith_inst_retired.128b_packed_double," + freq + ",CARM_SSEDP");
        event_strs.push_back("fp_arith_inst_retired.256b_packed_single," + freq + ",CARM_AVX2SP");
        event_strs.push_back("fp_arith_inst_retired.256b_packed_double," + freq + ",CARM_AVX2DP");
        event_strs.push_back("fp_arith_inst_retired.512b_packed_single," + freq + ",CARM_AVX512SP");
        event_strs.push_back("fp_arith_inst_retired.512b_packed_double," + freq + ",CARM_AVX512DP");
        event_strs.push_back("mem_inst_retired.any," + freq + ",CARM_MEM_LDST");
      }
#endif

      std::unordered_map<std::string, std::string> event_dict;

      for (std::string &event_str : event_strs) {
        std::vector<std::string> parts;
        boost::split(parts, event_str, boost::is_any_of(","));

        std::string event_name = parts[0];
        int period = std::stoi(parts[1]);
        std::string website_title = parts[2];

        PerfEvent event(event_name, period, buffer);
        profilers.push_back(std::make_unique<Perf>(perf_path, event, cpu_config,
                                                   event_name));

        event_dict[event_name] = website_title;
      }

      PipeAcceptor::Factory generic_acceptor_factory;

      for (int i = 0; i < profilers.size(); i++) {
        std::unique_ptr<Acceptor> acceptor =
          generic_acceptor_factory.make_acceptor(1);
        profilers[i]->set_acceptor(acceptor, server_buffer);
      }

      std::vector<pid_t> spawned_children;
      int to_return = 0;

      try {
        int code = start_profiling_session(profilers, command_elements, address, server_buffer,
                                           warmup, cpu_config, tmp_dir, spawned_children,
                                           event_dict, codes_dst, roofline_benchmark_path.get());

        auto end_time =
          ch::duration_cast<ch::milliseconds>(ch::system_clock::now().time_since_epoch()).count();

        if (code == 0) {
          fs::remove_all(tmp_dir);

          print("Done in " + std::to_string(end_time - start_time) + " ms in total! "
                "You can check the results directory now.", false, false);
        }

        to_return = code;
      } catch (ConnectionException &e) {
        print("I/O error has occurred! Exiting.", false, true);
        print("Details: " + std::string(e.what()), false, true);

        to_return = 2;
      } catch (std::exception &e) {
        print("A fatal error has occurred! If the issue persits, "
              "please contact the Adaptyst developers, citing \"" +
              std::string(e.what()) + "\".", false, true);

        to_return = 2;
      }

      for (auto &pid : spawned_children) {
        int status = waitpid(pid, nullptr, WNOHANG);

        if (status == 0) {
          kill(pid, SIGTERM);
        }
      }

      return to_return;
    }
  }
};
