// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#ifndef ENTRYPOINT_HPP_
#define ENTRYPOINT_HPP_

/**
   Adaptyst namespace.
*/
namespace aperf {
  /**
     The version of Adaptyst.
  */
  extern const char *version;

  int main_entrypoint(int argc, char **argv);
};

#endif
