// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#ifndef SERVER_ENTRYPOINT_HPP_
#define SERVER_ENTRYPOINT_HPP_

/**
   Adaptyst namespace.
*/
namespace aperf {
#ifndef ENTRYPOINT_HPP_
  /**
     The version of Adaptyst.
  */
  extern const char *version;
#endif
  int server_entrypoint(int argc, char **argv);
};

#endif
