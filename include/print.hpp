// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#ifndef PRINT_HPP_
#define PRINT_HPP_

#include "system.hpp"
#include <mutex>
#include <string>

namespace adaptyst {
  class Terminal {
  private:
    std::mutex mutex;
    std::mutex log_mutex;
    bool batch;
    bool formatted;
    int last_line_len;
    std::string version;
    std::unordered_map<Identifiable *,
                       std::unordered_map<std::string, std::ofstream> > log_streams;
    fs::path log_dir;
    Terminal(bool batch, bool formatted, std::string version,
             fs::path log_dir);

  public:
    static std::unique_ptr<Terminal> instance;
    static void init(bool batch, bool formatted, std::string version,
                     fs::path log_dir);

    void print_notice();
    void log(std::string message, Identifiable *source,
             std::string log_type);
    void print(std::string message, bool sub, bool error,
               bool same_line = false);
    void print(std::string message, bool sub, bool error,
               Identifiable *source, std::string log_type);
    const char *get_log_dir();
    void set_log_dir(fs::path log_dir);
  };
};

#endif
