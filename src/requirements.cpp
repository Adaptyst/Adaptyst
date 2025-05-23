//# Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#include "requirements.hpp"
#include "print.hpp"
#include <fstream>
#include <regex>
#include <boost/process.hpp>

#ifdef LIBNUMA_AVAILABLE
#include <numa.h>
#endif

namespace adaptyst {
  /**
     Constructs a PerfEventKernelSettingsReq object.

     @param max_stack  Indicates where the value of kernel.perf_event_max_stack
                       should be written to.
  */
  PerfEventKernelSettingsReq::PerfEventKernelSettingsReq(int &max_stack) : max_stack(max_stack) {}

  std::string PerfEventKernelSettingsReq::get_name() {
    return "Adequate values of kernel.perf_event settings";
  }

  bool PerfEventKernelSettingsReq::check_internal() {
    // kernel.perf_event_max_stack
    std::ifstream max_stack("/proc/sys/kernel/perf_event_max_stack");

    if (!max_stack) {
      print("Could not check the value of kernel.perf_event_max_stack!",
            true, true);
      return false;
    }

    int max_stack_value;
    max_stack >> max_stack_value;

    max_stack.close();

    if (max_stack_value < 1024) {
      print("kernel.perf_event_max_stack is less than 1024. Adaptyst will "
            "crash because of this, so stopping here. Please run \"sysctl "
            "kernel.perf_event_max_stack=1024\" (or the same command with "
            "a number larger than 1024).", true, true);
      return false;
    } else {
      this->max_stack = max_stack_value;
      print("Note that stacks with more than " + std::to_string(max_stack_value) +
            " entries/entry *WILL* be broken in your results! To avoid that, run "
            "\"sysctl kernel.perf_event_max_stack=<larger value>\".", true, false);
      print("Remember that max stack values larger than 1024 are currently *NOT* "
            "supported for off-CPU stacks (they will be capped at 1024 entries).",
            true, false);
    }

    // Done, everything's good!
    return true;
  };

  std::string NUMAMitigationReq::get_name() {
    return "NUMA balancing not interfering with profiling";
  }

  bool NUMAMitigationReq::check_internal() {
    fs::path numa_balancing_path("/proc/sys/kernel/numa_balancing");

    if (!fs::exists(numa_balancing_path)) {
      print("kernel.numa_balancing does not seem to exist, so assuming "
            "no NUMA on this machine. Note that if you actually have "
            "NUMA, you may get broken stacks!", true, false);
      return true;
    }

    std::ifstream numa_balancing(numa_balancing_path);

    if (!numa_balancing) {
      print("Could not check the value of kernel.numa_balancing!",
            true, true);
      return false;
    }

    int numa_balancing_value;
    numa_balancing >> numa_balancing_value;

    numa_balancing.close();

    if (numa_balancing_value == 1) {
#ifdef LIBNUMA_AVAILABLE
      unsigned long mask = *numa_get_membind()->maskp;
      int count = 0;

      while (mask > 0 && count <= 1) {
        if (mask & 0x1) {
          count++;
        }

        mask >>= 1;
      }

      if (count > 1) {
        print("NUMA balancing is enabled and Adaptyst is running on more "
              "than 1 NUMA node!",
              true, true);
        print("As this will result in broken stacks, Adaptyst will not run.",
              true, true);
        print("Please disable balancing by running \"sysctl "
              "kernel.numa_balancing=0\" or "
              "bind Adaptyst at least memory-wise "
              "to a single NUMA node, e.g. through numactl.",
              true, true);
        return false;
      }
#else
      print("NUMA balancing is enabled, but Adaptyst is compiled without "
            "libnuma support, so it cannot determine on how many NUMA nodes "
            "it is running!", true, true);
      print("As this may result in broken stacks, Adaptyst will not run.",
            true, true);
      print("Please disable balancing by running \"sysctl "
            "kernel.numa_balancing=0\" or "
            "recompile Adaptyst with libnuma support, followed by "
            "binding the tool at least memory-wise "
            "to a single NUMA node (e.g. through numactl).", true, true);
      return false;
#endif
    }

    return true;
  }
};
