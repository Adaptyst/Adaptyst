// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#include "print.hpp"
#include <iostream>
#include <mutex>

namespace adaptyst {
  Terminal *Terminal::instance = nullptr;

  Terminal::Terminal(bool batch, bool formatted, std::string version,
                     fs::path log_dir) {
    if (Terminal::instance) {
      throw std::runtime_error("Only one instance of Terminal can be constructed!");
    }

    this->batch = batch;
    this->formatted = formatted;
    this->version = version;
    this->last_line_len = 0;
    this->log_dir = log_dir;
    Terminal::instance = this;
  }

  /**
     Prints the GNU GPL v2 notice.
  */
  void Terminal::print_notice() {
    {
      std::unique_lock lock(this->mutex);

      std::cout << "Adaptyst " << this->version << std::endl;
      std::cout << "Copyright (C) CERN." << std::endl;
      std::cout << std::endl;
      std::cout
          << "This program is free software; you can redistribute it and/or"
          << std::endl;
      std::cout << "modify it under the terms of the GNU General Public License"
                << std::endl;
      std::cout
          << "as published by the Free Software Foundation; only version 2."
          << std::endl;
      std::cout << std::endl;
      std::cout
          << "This program is distributed in the hope that it will be useful,"
          << std::endl;
      std::cout
          << "but WITHOUT ANY WARRANTY; without even the implied warranty of"
          << std::endl;
      std::cout
          << "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"
          << std::endl;
      std::cout << "GNU General Public License for more details." << std::endl;
      std::cout << std::endl;
      std::cout
          << "You should have received a copy of the GNU General Public License"
          << std::endl;
      std::cout << "along with this program; if not, write to the Free Software"
                << std::endl;
      std::cout << "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,"
                << std::endl;
      std::cout << "MA 02110-1301, USA." << std::endl;
      std::cout << std::endl;
    }

    this->print("All logs are streamed to and saved inside " +
                this->log_dir.string() + " in form of "
                "\"<entity/node ID>_<log type>.log\".",
                false, false);
  }

  void Terminal::log(std::string message, Identifiable *source,
                     std::string log_type) {
    std::ofstream *stream = nullptr;

    {
      std::unique_lock lock(this->log_mutex);

      if (log_streams.find(source) == log_streams.end()) {
        log_streams[source] = std::unordered_map<std::string, std::ofstream>();
      }

      if (log_streams[source].find(log_type) == log_streams[source].end()) {
        log_streams[source][log_type] =
          std::ofstream(this->log_dir / (source->get_id() + "_" + log_type + ".log"));
      }

      stream = &log_streams[source][log_type];
    }

    if (!stream) {
      return;
    }

    if (!*stream) {
      return;
    }

    *stream << message << std::endl;
    stream->flush();
  }

  /**
     Prints a message.

     @param message   A string to be printed.
     @param sub       Indicates whether this message belongs to
                      a subsection (i.e. whether it should be printed
                      with the "->" prefix instead of "==>").
     @param error     Indicates whether this message is an error.
     @param same_line Indicates whether this message should be printed
                      in the current terminal line rather than in a
                      new line. Ignored if the batch mode is enabled.
  */
  void Terminal::print(std::string message, bool sub, bool error,
                       bool same_line) {
    std::unique_lock lock(this->mutex);
    int new_len = 0;

    if (same_line && !this->batch) {
      std::cout << "\r";
    }

    if (sub) {
      std::cout << (this->formatted ? (error ? "\033[0;31m" : "\033[0;36m") : "") << "-> ";
      new_len += 3;
    } else {
      std::cout << (this->formatted ? (error ? "\033[1;31m" : "\033[1;32m") : "") << "==> ";
      new_len += 4;
    }

    std::cout << message << (this->formatted ? "\033[0m" : "");
    new_len += message.length();

    if (same_line && !this->batch) {
      for (int i = 0; i < this->last_line_len - new_len; i++) {
        std::cout << " ";
      }

      std::cout.flush();
    } else {
      std::cout << std::endl;
    }

    this->last_line_len = new_len;
  }

  void Terminal::print(std::string message, bool sub, bool error,
                       Identifiable *source, std::string log_type) {
    this->log((error ? "[ERROR] " : std::string()) + (sub ? "-> " : "==> ") + message,
              source, log_type);
  }
};
