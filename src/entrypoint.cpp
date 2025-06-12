// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#include "entrypoint.hpp"
#include "print.hpp"
#include "cmd.hpp"
#include "system.hpp"
#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/predef.h>
#include <regex>
#include <sys/wait.h>
#include <filesystem>
#include <pybind11/embed.h>

#ifndef ADAPTYST_CONFIG_FILE
#define ADAPTYST_CONFIG_FILE ""
#endif

namespace adaptyst {
  namespace ch = std::chrono;
  namespace fs = std::filesystem;
  namespace py = pybind11;
  using namespace std::chrono_literals;

  bool quiet;

  // Commented out as this is not currently used
  // ---------------------
  // /**
  //    A class validating whether a supplied command-line
  //    option is equal to or larger than a given value.
  // */
  // class OnlyMinRange : public CLI::Validator {
  // public:
  //   OnlyMinRange(int min) {
  //     func_ = [=](const std::string &arg) -> std::string {
  //       if (!std::regex_match(arg, std::regex("^-?[0-9]+$")) ||
  //           std::stoi(arg) < min) {
  //         return "The value must be a number equal to or greater than " +
  //           std::to_string(min);
  //       }

  //       return "";
  //     };
  //   }
  // };

  int main_entrypoint(int argc, char **argv) {
    CLI::App app("Adaptyst: a performance analysis tool");
    app.formatter(std::make_shared<PrettyFormatter>());

    app.set_version_flag("-v,--version", adaptyst::version);

    std::string system_def_dir = "";
    app.add_option("-s,--system", system_def_dir, "Path to the definition "
                   "file of a computer system (required). See the "
                   "documentation to learn how to write a computer system "
                   "definition file.")
      ->check(CLI::ExistingFile)
      ->option_text("FILE")
      ->required();

    std::string out_dir = "";
    app.add_option("-o,--output", out_dir, "Path to the directory where "
                   "analysis results should be saved")
      ->check(CLI::NonexistentPath)
      ->option_text("PATH")
      ->required();

    std::string codes_dst = "";
    app.add_option("-c,--codes", codes_dst, "Send the newline-separated list "
                   "of detected source code files to a specified destination "
                   "rather than pack the code files on the same entity where "
                   "an analysed program is run. The value can be either "
                   "\"file:<path>\" (i.e. the list is saved to <path> "
                   "and can be then read e.g. by adaptyst-code) or "
                   "\"fd:<number>\" (i.e. the list is written to a specified "
                   "file descriptor).")
      ->check([](const std::string &arg) -> std::string {
        if (!std::regex_match(arg, std::regex("^(file\\:.+|fd:\\d+)$"))) {
          return "The value must be in form of \"file:<path>\" or "
            "\"fd:<number>\"";
        }

        return "";
      })
      ->option_text("TYPE[:ARG]");

    bool batch = false;
    app.add_flag("--batch", batch, "Run Adaptyst in batch mode "
                 "(i.e. with no in-line updates)");

    bool no_format = false;
    app.add_flag("--no-format", no_format, "Do not use any non-standard "
                 "terminal formatting");

    std::string footer =
      "If you want to change the paths of the system-wide and local Adaptyst\n"
      "configuration files, set the environment variables ADAPTYST_CONFIG and\n"
      "ADAPTYST_LOCAL_CONFIG respectively to values of your choice. Similarly,\n"
      "you can set the ADAPTYST_SCRIPT_DIR environment variable to change the path\n"
      "where Adaptyst looks for workflow and system modules.";

    app.footer(footer);

    bool call_split_unix = true;

    for (int i = 0; i < argc; i++) {
      if (strcmp(argv[i], "--") == 0) {
        call_split_unix = false;
        break;
      }
    }

    std::vector<std::string> command_parts;
    std::vector<std::string> command_elements;
    app.add_option("COMMAND", command_parts, "Command to be analysed (required)")
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
      ->take_all()
      ->required();

    CLI11_PARSE(app, argc, argv);

    fs::path library_path(ADAPTYST_SCRIPT_PATH);

    if (getenv("ADAPTYST_SCRIPT_DIR")) {
      library_path = fs::path(getenv("ADAPTYST_SCRIPT_DIR"));
    }

    fs::path system_config_path(ADAPTYST_CONFIG_FILE);
    fs::path local_config_path = fs::path(getenv("HOME")) /
      ".adaptyst" / "adaptyst.conf";

    if (getenv("ADAPTYST_CONFIG")) {
      system_config_path = fs::path(getenv("ADAPTYST_CONFIG"));
    }

    if (getenv("ADAPTYST_LOCAL_CONFIG")) {
      local_config_path = fs::path(getenv("ADAPTYST_LOCAL_CONFIG"));
    }

    pid_t current_pid = getpid();
    fs::path tmp_dir = fs::temp_directory_path() /
      ("adaptyst.pid." + std::to_string(current_pid));

    try {
      if (fs::exists(tmp_dir)) {
        fs::remove_all(tmp_dir);
      }

      fs::create_directories(tmp_dir);
      fs::create_directories(tmp_dir / "log");
      fs::create_directories(tmp_dir / "system");
    } catch (fs::filesystem_error) {
      std::cerr << "Could not create " + tmp_dir.string() + "! Exiting.";
      return 1;
    }

    Terminal terminal(batch, !no_format, adaptyst::version, tmp_dir / "log");
    terminal.print_notice();

    auto start_time =
      ch::duration_cast<ch::milliseconds>(ch::system_clock::now().time_since_epoch()).count();

    terminal.print("Reading config file(s)...", false, false);

    std::unordered_map<std::string, std::string> config;

    auto read_config = [&terminal](fs::path config_path,
                                   std::unordered_map<std::string, std::string> &result) {
      std::ifstream stream(config_path);

      if (!stream) {
        terminal.print("Cannot open or find " + config_path.string() + ", ignoring.",
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
          terminal.print("Syntax error in line " + std::to_string(cur_line) + " of " +
                         config_path.string() + "!", true, true);
          return false;
        }

        result[match[1]] = match[2];
        cur_line++;
      }

      terminal.print("Successfully read " + config_path.string(), true, false);
      return true;
    };

    if (!read_config(system_config_path, config) ||
        !read_config(local_config_path, config)) {
      return 2;
    }

    // print("Checking CPU specification...", false, false);

    // CPUConfig cpu_config = get_cpu_config(post_process);

    // if (!cpu_config.is_valid()) {
    //   return 1;
    // }

    // cpu_set_t cpu_set = cpu_config.get_cpu_profiler_set();
    // sched_setaffinity(0, sizeof(cpu_set), &cpu_set);

    std::vector<pid_t> spawned_children;
    int to_return = 0;

    try {
      System system(system_def_dir, out_dir, library_path,
                    local_config_path, "", tmp_dir / "system");
      py::scoped_interpreter py_interpreter;

      int code = 0; // Replace with proper starting up

      auto end_time =
        ch::duration_cast<ch::milliseconds>(ch::system_clock::now().time_since_epoch()).count();

      if (code == 0) {
        fs::remove_all(tmp_dir);

        unsigned long long elapsed = end_time - start_time;
        std::string elapsed_str;

        if (elapsed >= 1000) {
          int ms = elapsed % 1000;
          elapsed /= 1000;

          elapsed_str = std::to_string(elapsed) + ".";

          if (ms >= 100) {
            elapsed_str += std::to_string(ms);
          } else if (ms >= 10) {
            elapsed_str += "0" + std::to_string(ms);
          } else {
            elapsed_str += "00" + std::to_string(ms);
          }

          elapsed_str += " s";
        } else {
          elapsed_str = std::to_string(elapsed) + " ms";
        }

        terminal.print("Done in " + elapsed_str + " in total! "
                       "You can inspect the results now.", false, false);
      }

      to_return = code;
    } catch (std::exception &e) {
      terminal.print("A fatal error has occurred! If the issue persits, "
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
};
