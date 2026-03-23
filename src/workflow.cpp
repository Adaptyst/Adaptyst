// SPDX-FileCopyrightText: 2026 CERN
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "workflow.hpp"

namespace adaptyst {
  Workflow::Workflow(std::vector<std::string> command_elements) {
    this->is_command = true;
    this->command_elements = command_elements;
  }

  Workflow::Workflow(fs::path workflow_file) {
    this->is_command = false;
  }

  bool Workflow::is_command_only() {
    return this->is_command;
  }

  std::vector<std::string> Workflow::get_command_elements() {
    if (!this->is_command_only()) {
      throw std::runtime_error("get_command_elements() can't be called "
                               "if the workflow isn't in the "
                               "single-command mode");
    }

    return this->command_elements;
  }

  std::unique_ptr<IR> WorkflowCompilerMLIR::compile(Workflow &workflow) {
    throw std::runtime_error("WorkflowCompilerMLIR is not implemented yet");
  }

  std::unique_ptr<IR> WorkflowCompilerSingleCmd::compile(Workflow &workflow) {
    return std::make_unique<SingleCmd>(workflow.get_command_elements());
  }
};
