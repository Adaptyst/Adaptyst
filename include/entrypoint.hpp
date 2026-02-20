// SPDX-FileCopyrightText: 2026 CERN
// SPDX-License-Identifier: LGPL-3.0-or-later

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
