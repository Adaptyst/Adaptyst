// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#include "server/entrypoint.hpp"
#include "entrypoint.hpp"

int main(int argc, char **argv) {
#ifdef SERVER_ONLY
  return aperf::server_entrypoint(argc, argv);
#else
  return aperf::main_entrypoint(argc, argv);
#endif
}
