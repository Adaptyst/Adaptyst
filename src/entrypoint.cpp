// SPDX-FileCopyrightText: 2026 CERN
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "entrypoint.hpp"
#include "print.hpp"
#include "cmd.hpp"
#include "system.hpp"
#include "adaptyst/output.hpp"
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

#define TM_YEAR_ALIGN 1900

namespace adaptyst {
  namespace ch = std::chrono;
  namespace fs = std::filesystem;
  namespace py = pybind11;
  using namespace std::chrono_literals;

  bool quiet;

  /**
     A class validating whether a supplied command-line
     option is equal to or larger than a given value.
  */
  class OnlyMinRange : public CLI::Validator {
  public:
    OnlyMinRange(int min) {
      func_ = [=](const std::string &arg) -> std::string {
        if (!std::regex_match(arg, std::regex("^-?[0-9]+$")) ||
            std::stoi(arg) < min) {
          return "The value must be a number equal to or greater than " +
            std::to_string(min);
        }

        return "";
      };
    }
  };

  int main_entrypoint(int argc, char **argv) {
    CLI::App app("Adaptyst: a performance analysis tool");
    app.formatter(std::make_shared<PrettyFormatter>());

    app.set_version_flag("-v,--version", adaptyst::version);

    bool list_modules = false;
    app.add_flag("--modules", list_modules, "List in detail all "
                 "installed system modules and exit");

    bool list_plugins = false;
    app.add_flag("--plugins", list_plugins, "List in detail all "
                 "installed workflow plugins and exit");

    bool print_info = false;
    app.add_flag("--info", print_info, "Print information about "
                 "various paths used by Adaptyst such as the "
                 "module dir(s)");

    std::string module_help = "";
    app.add_option("-m,--module-help", module_help, "Print the help "
                   "message of a given module and exit")
      ->option_text("MODULE");

    std::string plugin_help = "";
    app.add_option("-p,--plugin-help", plugin_help, "Print the help "
                   "message of a given plugin and exit")
      ->option_text("PLUGIN");

    bool is_command = false;
    app.add_flag("-d,--command", is_command, "Indicates that a command "
                 "will be provided for analysis rather than "
                 "the path to a YAML file defining a workflow to be "
                 "analysed");

    std::string system_def_dir = "";
    app.add_option("-s,--system", system_def_dir, "Path to the definition "
                   "file of a computer system (required). See the "
                   "documentation to learn how to write a computer system "
                   "definition file.")
      ->check(CLI::ExistingFile)
      ->option_text("FILE");

    std::string out_dir = "";
    app.add_option("-o,--output", out_dir, "Path to the directory where "
                   "analysis results should be saved (default: "
                   "adaptyst_<UTC timestamp>__<positive integer>)")
      ->check(CLI::NonexistentPath)
      ->option_text("PATH");

    std::string label = "";
    app.add_option("-l,--label", label, "Label of the performance "
                   "analysis session (default: "
                   "adaptyst_<UTC timestamp>__<positive integer>)")
      ->option_text("TEXT");

    unsigned int buf_size = 1024;
    app.add_option("--buffer", buf_size, "Size of buffer for internal "
                   "communication in bytes (default: 1024)")
      ->option_text("UINT")
      ->check(OnlyMinRange(0));

    // no_inject will be fully implemented when process injection mechanism
    // is implemented
    bool no_inject = false;
    // app.add_flag("-n,--no-inject", no_inject, "Do not use the process "
    //              "injection mechanism (some modules will not work with "
    //              "this flag)");

    // std::string codes_dst = "";
    // app.add_option("-c,--codes", codes_dst, "Send the newline-separated list "
    //                "of detected source code files to a specified destination "
    //                "rather than pack the code files on the same entity where "
    //                "an analysed program is run. The value can be either "
    //                "\"file:<path>\" (i.e. the list is saved to <path> "
    //                "and can be then read e.g. by adaptyst-code) or "
    //                "\"fd:<number>\" (i.e. the list is written to a specified "
    //                "file descriptor).")
    //   ->check([](const std::string &arg) -> std::string {
    //     if (!std::regex_match(arg, std::regex("^(file\\:.+|fd:\\d+)$"))) {
    //       return "The value must be in form of \"file:<path>\" or "
    //         "\"fd:<number>\"";
    //     }

    //     return "";
    //   })
    //   ->option_text("TYPE[:ARG]");

    bool no_format = false;
    app.add_flag("--no-format", no_format, "Do not use any non-standard "
                 "terminal formatting");

    std::string footer =
      "If you want to change the paths of the system-wide and local Adaptyst\n"
      "configuration files, set the environment variables ADAPTYST_CONFIG and\n"
      "ADAPTYST_LOCAL_CONFIG respectively to values of your choice. Similarly,\n"
      "you can set the ADAPTYST_MODULE_DIRS environment variable to change the\n"
      "colon-separated paths where Adaptyst looks for workflow plugins and system\n"
      "modules. You can also set ADAPTYST_MISC_DIR to change the path where Adaptyst\n"
      "looks for its support files.";

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
    app.add_option("COMMAND/PATH", command_parts, "Path to a "
                   "workflow to be analysed (required). If -d is set, a command "
                   "to be analysed should be provided instead.")
      ->check([&call_split_unix, &command_elements, &is_command](const std::string &arg) {
        if (is_command) {
          const char *not_valid = "The command you have provided is not a valid one!";

          if (arg.empty()) {
            return "";
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
        } else {
          if (arg.empty()) {
            return "";
          } else if (command_elements.size() > 0) {
            return "You must provide a single path only.";
          } else {
            if (!fs::exists(arg)) {
              return "The path you have provided does not exist!";
            }

            fs::path resolved = fs::canonical(arg);

            if (!fs::is_regular_file(resolved)) {
              return "The path you have provided does not point to a regular file!";
            }

            command_elements.push_back(resolved.string());
          }
        }

        return "";
      })
      ->option_text(" ")
      ->take_all();

    CLI11_PARSE(app, argc, argv);

    std::vector<fs::path> module_paths;

    if (getenv("ADAPTYST_MODULE_DIRS")) {
      std::vector<std::string> paths;
      boost::split(paths, getenv("ADAPTYST_MODULE_DIRS"), boost::is_any_of(":"));
      for (std::string &path : paths) {
        module_paths.push_back(fs::path(path));
      }
    } else {
      module_paths.push_back(ADAPTYST_MODULE_PATH);
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

    std::string pythonpath = ADAPTYST_MISC_PATH;

    if (getenv("ADAPTYST_MISC_DIR")) {
      pythonpath = fs::path(getenv("ADAPTYST_MISC_DIR"));
    }

    if (list_modules || list_plugins) {
      int to_return = 0;

      if (list_modules) {
        try {
          auto modules = Module::get_all_modules(module_paths);

          if (modules.empty()) {
            std::cout << "No modules are installed." << std::endl;
          } else {
            std::cout << "Installed modules:" << std::endl;
            for (auto &sys_module : modules) {
              std::string name = sys_module->get_name();
              std::string version = sys_module->get_version();
              fs::path path = sys_module->get_lib_path();

              std::cout << "* " << name << " v" << version << " (";
              std::cout << path.string() << ")" << std::endl;
            }
          }
        } catch (std::exception &e) {
          std::cerr << "An error occurred when querying installed modules.";
          std::cerr << std::endl;
          std::cerr << "Details: " << e.what() << std::endl;
          to_return = 2;
        }

        if (list_plugins) {
          std::cout << std::endl;
        }
      }

      if (list_plugins) {
        std::cout << "The full functionality of plugins is not implemented yet.";
        std::cout << std::endl;
        std::cout << "You can currently only analyse commands via the -d option.";
        std::cout << std::endl;
      }

      return to_return;
    } else if (print_info) {
      std::cout << "Path where Adaptyst miscellaneous files can be found ";
      std::cout << "(changable via ADAPTYST_MISC_DIR env variable):" << std::endl;
      std::cout << fs::path(pythonpath).string() << std::endl << std::endl;

      std::cout << "Path(s) where Adaptyst modules can be found ";
      std::cout << "(changable via ADAPTYST_MODULE_DIRS env variable):" << std::endl;

      for (auto &path : module_paths) {
        std::cout << path.string() << std::endl;
      }

      std::cout << std::endl;

      std::cout << "Path of the system-wide Adaptyst configuration file ";
      std::cout << "(changable via ADAPTYST_CONFIG env variable):" << std::endl;
      std::cout << system_config_path.string() << std::endl << std::endl;

      std::cout << "Path of the local Adaptyst configuration file ";
      std::cout << "(changable via ADAPTYST_LOCAL_CONFIG env variable):" << std::endl;
      std::cout << local_config_path.string() << std::endl;

      return 0;
    } else if (module_help != "" && plugin_help != "") {
      std::cerr << "-m and -p simultaneously are not supported" << std::endl;
      return 1;
    } else if (module_help != "") {
      try {
        Module sys_module(module_help, module_paths);

        std::string name = sys_module.get_name();
        std::string version = sys_module.get_version();

        std::cout << name << " v" << version << std::endl << std::endl;
        std::cout << "Available options:" << std::endl;
        std::cout << "------------------";

        for (auto &option_metadata : sys_module.get_all_options()) {
          std::cout << std::endl;

          if (option_metadata.second.array_type == NONE &&
              option_metadata.second.type == NONE) {
            std::cout << option_metadata.first;
            std::cout << " (invalid, check with the module developers)";
            std::cout << std::endl;
            continue;
          }

          std::cout << option_metadata.first << " (";

          switch (option_metadata.second.array_type) {
          case INT:
            std::cout << "array of integers";
            break;

          case UNSIGNED_INT:
            std::cout << "array of unsigned integers";
            break;

          case STRING:
            std::cout << "array of strings";
            break;

          case BOOL:
            std::cout << "array of booleans";
            break;

          case NONE:
            break;
          }

          switch (option_metadata.second.type) {
          case INT:
            std::cout << "integer";
            break;

          case UNSIGNED_INT:
            std::cout << "unsigned integer";
            break;

          case STRING:
            std::cout << "string";
            break;

          case BOOL:
            std::cout << "boolean";
            break;

          case NONE:
            break;
          }

          std::cout << "):" << std::endl;

          std::vector<std::string> parts;
          boost::split(parts, option_metadata.second.help, boost::is_any_of(" "));

          int characters_printed_in_line = 0;
          std::cout << "   ";

          for (int i = 0; i < parts.size(); i++) {
            std::string to_print = parts[i];

            if (i < parts.size() - 1) {
              to_print += " ";
            }

            if (characters_printed_in_line > 0 &&
                characters_printed_in_line + to_print.length() >= 80 - 3) {
              std::cout << std::endl << "   ";
              characters_printed_in_line = 0;
            }

            std::cout << to_print;
            characters_printed_in_line += to_print.length();
          }

          std::cout << std::endl;
        }
      } catch (std::exception &e) {
        if (std::string(e.what()).ends_with("Could not find the module!")) {
          std::cerr << "The specified module could not be found!" << std::endl;
        } else {
          std::cerr << "An error occurred! Details: " << std::endl;
          std::cerr << e.what() << std::endl;
        }

        return 2;
      }

      return 0;
    } else if (plugin_help != "") {
      std::cout << "The full functionality of plugins is not implemented yet.";
      std::cout << std::endl;
      std::cout << "You can currently only analyse commands via the -d option.";
      std::cout << std::endl;
      return 0;
    } else if (system_def_dir == "") {
      std::cerr << "The definition file of a computer system is required! (use -s)";
      std::cerr << std::endl;
      return 1;
    } else if (command_elements.empty()) {
      std::cerr << "A workflow to be analysed is required!";
      std::cerr << std::endl;
      return 1;
    } else if (!is_command) {
      std::cerr << "Only analysing commands is supported at the moment, please use -d.";
      std::cerr << std::endl;
      return 1;
    }

    pid_t current_pid = getpid();
    fs::path tmp_dir = fs::temp_directory_path() /
      ("adaptyst.pid." + std::to_string(current_pid));

    try {
      if (fs::exists(tmp_dir)) {
        fs::remove_all(tmp_dir);
      }

      fs::create_directories(tmp_dir);
      fs::create_directories(tmp_dir / "system");
      fs::create_directories(tmp_dir / "log");
    } catch (fs::filesystem_error) {
      std::cerr << "Could not create " + tmp_dir.string() + ", " +
        (tmp_dir / "system").string() + ", or " +
        (tmp_dir / "log").string() + "! Exiting.";
      return 1;
    }

    const time_t t = std::time(nullptr);
    struct tm *tm = std::gmtime(&t);

    std::ostringstream timestamp_stream;
    timestamp_stream << std::put_time(tm, "%Y_%m_%d_%H_%M_%S");
    std::string timestamp = timestamp_stream.str();

    int index = 1;

    do {
      out_dir = "adaptyst_" + timestamp + "__" + std::to_string(index++);
    } while (fs::exists(out_dir));

    Path out_dir_obj(out_dir);

    out_dir_obj.set_metadata<int>("year",
                                  TM_YEAR_ALIGN + tm->tm_year, false);
    out_dir_obj.set_metadata<int>("month", tm->tm_mon + 1, false);
    out_dir_obj.set_metadata<int>("day", tm->tm_mday, false);
    out_dir_obj.set_metadata<int>("hour", tm->tm_hour, false);
    out_dir_obj.set_metadata<int>("minute", tm->tm_min, false);
    out_dir_obj.set_metadata<int>("second", tm->tm_sec, false);

    char hostname[HOST_NAME_MAX + 1];

    if (gethostname(hostname, HOST_NAME_MAX + 1) == -1) {
      out_dir_obj.set_metadata<std::string>("executor", "(unknown)", false);
    } else {
      out_dir_obj.set_metadata<std::string>("executor", std::string(hostname), false);
    }

    out_dir_obj.set_metadata<std::string>("label", label.empty() ? out_dir : label, false);
    out_dir_obj.save_metadata();

    Terminal::init(false, !no_format, adaptyst::version,
                   fs::path(out_dir) / "log");
    Terminal &terminal = *Terminal::instance;

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

    std::vector<pid_t> spawned_children;
    int to_return = 0;

    const char *existing_pythonpath = getenv("PYTHONPATH");

    if (existing_pythonpath) {
      pythonpath += ":" + std::string(existing_pythonpath);
    }

    setenv("PYTHONPATH", pythonpath.c_str(), 1);
    py::scoped_interpreter py_interpreter;

    try {
      terminal.print("Reading the computer system definition file...", false, false);
      System system(system_def_dir, fs::path(out_dir) / "system", module_paths,
                    local_config_path, tmp_dir / "system", no_inject, buf_size);

      terminal.print("Making an SDFG of the command/workflow...", false, false);

      py::module_ gen_sdfg = py::module_::import("gen_sdfg");
      py::list cmd_list;

      for (auto &element : command_elements) {
        cmd_list.append(element);
      }

      py::object sdfg = is_command ?
        gen_sdfg.attr("gen_sdfg_from_cmd")(cmd_list) :
        gen_sdfg.attr("gen_sdfg_from_yml")(command_elements[0]);

      system.set_sdfg(sdfg.cast<std::string>());

      terminal.print("Running performance analysis...", false, false);

      system.process();

      auto end_time =
        ch::duration_cast<ch::milliseconds>(ch::system_clock::now().time_since_epoch()).count();

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

      terminal.print("Done in " + elapsed_str + " in total!", false, false);
      to_return = 0;
    } catch (py::error_already_set &e) {
      std::string msg(e.what());

      if (e.matches(PyExc_ModuleNotFoundError)) {
        if (msg.find("'gen_sdfg'") != std::string::npos) {
          terminal.print("Could not find the Adaptyst Python module! Please check "
                         "your Adaptyst library path.", true, true);
        } else if (msg.find("'dace'") != std::string::npos) {
          terminal.print("DaCe could not be found! Please set "
                         "it up first (either system-wide or in a "
                         "Python virtual environment).",
                         true, true);
        } else if (msg.find("'yaml'") != std::string::npos) {
          terminal.print("PyYAML could not be found! Please set "
                         "it up first (either system-wide or in a "
                         "Python virtual environment).",
                         true, true);
        } else {
          terminal.print("The Adaptyst Python module has thrown an error:\n" +
                         msg, true, true);
        }
      } else {
        terminal.print("The Adaptyst Python module has thrown an error:\n" +
                       msg, true, true);
      }

      to_return = 2;
    } catch (std::runtime_error &e) {
      terminal.print(e.what(), true, true);
      to_return = 2;
    } catch (std::exception &e) {
      terminal.print("A fatal error has occurred! If the issue persits, "
                     "please contact the Adaptyst developers, citing \"" +
                     std::string(e.what()) + "\".", false, true);

      to_return = 2;
    }

    if (to_return == 0) {
      terminal.print("The results are available in " + fs::absolute(out_dir).string(),
                     true, false);
    } else {
      terminal.print("The incomplete results are available in " + fs::absolute(out_dir).string(),
                     true, false);
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
