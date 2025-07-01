// Adaptyst: a performance analysis tool
// Copyright (C) CERN. See LICENSE for details.

#ifndef ADAPTYST_HW_H_
#define ADAPTYST_HW_H_

#ifndef __cplusplus
#include <stdbool.h>
#endif

#define ADAPTYST_ERR_MODULE_NOT_FOUND 1
#define ADAPTYST_ERR_OUT_OF_MEMORY 2
#define ADAPTYST_ERR_EXCEPTION 3
#define ADAPTYST_ERR_TERMINAL_NOT_INITIALISED 4

#ifdef __cplusplus
extern "C" {
#endif
  typedef enum {
    INT = 1,
    STRING = 2,
    UNSIGNED_INT = 3,
    BOOL = 4,
    NONE = 0
  } option_type;

  typedef struct {
    option_type type;
    void *data;
    unsigned int len;
  } option;

  typedef enum {
    LINUX_PROCESS = 0
  } profile_type;

  typedef struct {
    profile_type type;
    union {
      int pid;
    } data;
  } profile_info;

  typedef unsigned int amod_t;

  bool adaptyst_new_context(amod_t id, unsigned int size);
  void *adaptyst_get_context(amod_t id);
  option *adaptyst_get_option(amod_t id, const char *key);
  bool adaptyst_set_error(amod_t id, const char *msg);
  const char *adaptyst_get_log_dir(amod_t id);
  const char *adaptyst_get_node_id(amod_t id);
  bool adaptyst_log(amod_t id, const char *msg, const char *type);
  bool adaptyst_print(amod_t id, const char *msg, bool sub, bool error,
                      const char *type);
  const char *adaptyst_get_node_dir(amod_t id);
  profile_info *adaptyst_get_profile_info(amod_t id);
  bool adaptyst_set_profile_info(amod_t id, profile_info *info);
  bool adaptyst_is_directing_node(amod_t id);
  bool adaptyst_profile_notify(amod_t id);
  int adaptyst_profile_wait(amod_t id);
  bool adaptyst_process_src_paths(amod_t id, const char **paths, int n);
  const char *adaptyst_get_cpu_mask(amod_t id);
  const char *adaptyst_get_tmp_dir(amod_t id);
  const char *adaptyst_get_local_config_dir(amod_t id);
  bool adaptyst_set_will_profile(amod_t id, bool will_profile);
  bool adaptyst_has_in_tag(amod_t id, const char *tag);
  bool adaptyst_has_out_tag(amod_t id, const char *tag);
  const char ***adaptyst_get_in_tags(amod_t id);
  const char ***adaptyst_get_out_tags(amod_t id);

#if defined(ADAPTYST_MODULE_ENTRYPOINT) || !defined(ADAPTYST_INTERNAL)
  extern amod_t module_id;
#endif

#ifdef ADAPTYST_MODULE_ENTRYPOINT
  bool adaptyst_module_init();

  bool _adaptyst_module_init(amod_t id) {
    module_id = id;
    return adaptyst_module_init();
  }

  bool adaptyst_module_process(const char *sdfg);
  void adaptyst_module_close();
#endif
#ifdef __cplusplus
}
#endif

#endif
