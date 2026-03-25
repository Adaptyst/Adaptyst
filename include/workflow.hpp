// SPDX-FileCopyrightText: 2026 CERN
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef WORKFLOW_HPP_
#define WORKFLOW_HPP_

#include "ir.hpp"
#include <vector>
#include <string>
#include <filesystem>

namespace adaptyst {
  namespace fs = std::filesystem;

  class Workflow {
  private:
    bool is_command;
    std::vector<std::string> command_elements;

  public:
    Workflow(std::vector<std::string> command_elements);
    Workflow(fs::path workflow_file);
    bool is_command_only();
    std::vector<std::string> get_command_elements();
  };

  class WorkflowCompiler {
  public:
    virtual std::unique_ptr<IR> compile(Workflow &workflow) = 0;
  };

  class WorkflowCompilerMLIR : public WorkflowCompiler {
  public:
    std::unique_ptr<IR> compile(Workflow &workflow);
  };

  class WorkflowCompilerSingleCmd : public WorkflowCompiler {
  public:
    std::unique_ptr<IR> compile(Workflow &workflow);
  };
};

#endif
