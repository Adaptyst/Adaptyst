// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#ifndef REQUIREMENTS_HPP_
#define REQUIREMENTS_HPP_

#include "profiling.hpp"

namespace adaptyst {
  /**
     A class describing the requirement of the correct
     "perf"-specific kernel settings.

     At the moment, this is only kernel.perf_event_max_stack.
  */
  class PerfEventKernelSettingsReq : public Requirement {
  private:
    int &max_stack;

  protected:
    bool check_internal();

  public:
    PerfEventKernelSettingsReq(int &max_stack);
    std::string get_name();
  };

  /**
     A class describing the requirement of having proper
     NUMA-specific mitigations.

     The behaviour of this class depends on whether
     Adaptyst is compiled with libnuma support.
  */
  class NUMAMitigationReq : public Requirement {
  protected:
    bool check_internal();

  public:
    std::string get_name();
  };
};

#endif
