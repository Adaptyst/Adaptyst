// SPDX-FileCopyrightText: 2025 CERN
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ADAPTYST_HW_INJECT_H_
#define ADAPTYST_HW_INJECT_H_

#include <adaptyst/amod_t.h>
#include <adaptyst/inject_errors.h>

#define ADAPTYST_MODULE_OK 0
#define ADAPTYST_MODULE_ERR 1

#ifdef __cplusplus
extern "C" {
#endif
int adaptyst_send_data(amod_t id, char *buf, unsigned int n);
int adaptyst_receive_data(amod_t id, char *buf, unsigned int buf_size,
                          int *n);
int adaptyst_receive_data_timeout(amod_t id, char *buf, unsigned int buf_size,
                                  int *n, long timeout_seconds);
int adaptyst_send_string(amod_t id, const char *str);
int adaptyst_receive_string(amod_t id, const char **str);
int adaptyst_receive_string_timeout(amod_t id, const char **str, long timeout_seconds);
int adaptyst_send_data_nl(amod_t id, char *buf, unsigned int n);
int adaptyst_receive_data_nl(amod_t id, char *buf, unsigned int buf_size,
                             int *n);
int adaptyst_receive_data_timeout_nl(amod_t id, char *buf, unsigned int buf_size,
                                     int *n, long timeout_seconds);
int adaptyst_send_string_nl(amod_t id, const char *str);
int adaptyst_receive_string_nl(amod_t id, const char **str);
int adaptyst_receive_string_timeout_nl(amod_t id, const char **str, long timeout_seconds);
void adaptyst_set_error(const char *msg);
void adaptyst_set_error_nl(const char *msg);
unsigned long long adaptyst_get_timestamp(int *err);
#ifdef __cplusplus
}
#endif

#endif
