// SPDX-FileCopyrightText: 2025 CERN
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENTRYPOINT_HPP_
#define ENTRYPOINT_HPP_

/**
   Adaptyst namespace.
*/
namespace adaptyst {
  /**
     The version of Adaptyst.
  */
  extern const char *version;

  int main_entrypoint(int argc, char **argv);
};

#endif
