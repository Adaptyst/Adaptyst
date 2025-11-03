// SPDX-FileCopyrightText: 2025 CERN
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ADAPTYST_INJECT_H_
#define ADAPTYST_INJECT_H_

#include <adaptyst/amod_t.h>
#include <adaptyst/inject_errors.h>

#ifdef __cplusplus
extern "C" {
#endif
void adaptyst_set_print_errors(unsigned int print);
int adaptyst_init();
int adaptyst_init_custom_buf_size(unsigned int size);
const char **adaptyst_get_runtime_info();
char *adaptyst_get_error_msg();
int adaptyst_region_start(const char *name);
int adaptyst_region_end(const char *name);
void adaptyst_close();
#ifdef __cplusplus
}
#endif
  
#endif
