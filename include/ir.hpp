// SPDX-FileCopyrightText: 2026 CERN
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef IR_HPP_
#define IR_HPP_

#include <filesystem>
#include "adaptyst/hw.h"
#include "adaptyst/output.hpp"
#include "adaptyst/process.hpp"

namespace adaptyst {
  class IR {
  private:
    unsigned int type;
    bool compiled;

  protected:
    virtual void *get_c_data() = 0;
    virtual void _compile() = 0;
    virtual int _execute() = 0;

  public:
    IR(unsigned int type);
    ir to_c_type();
    void compile();
    std::unique_ptr<Process> execute();
  };

  class MLIR : public IR {
  private:
    std::unique_ptr<Path> output_dir;

  protected:
    void *get_c_data();
    void _compile();
    int _execute();

  public:
    MLIR(fs::path output_dir);
  };

  class SingleCmd : public IR {
  private:
    std::vector<char **> cmd_copies;
    std::vector<int> element_sizes;

  protected:
    void *get_c_data();
    void _compile();
    int _execute();

  public:
    SingleCmd(std::vector<std::string> elements);
    ~SingleCmd();
  };
};

#endif
