// SPDX-FileCopyrightText: 2025 CERN
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ADAPTYST_HW_H_
#define ADAPTYST_HW_H_

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <adaptyst/amod_t.h>

/**
   \def ADAPTYST_OK
   No error has occurred.
   Numerical value: 0

   \def ADAPTYST_ERR_MODULE_NOT_FOUND
   Error indicating that a module with the specified ID hasn't been found.
   Numerical value: 1

   \def ADAPTYST_ERR_OUT_OF_MEMORY
   Error indicating that there is no memory left.
   Numerical value: 2

   \def ADAPTYST_ERR_EXCEPTION
   Error indicating that a C++ exception occurred on the Adaptyst side.
   Numerical value: 3

   \def ADAPTYST_ERR_TERMINAL_NOT_INITIALISED
   Error indicating that the terimnal-related resources in Adaptyst haven't
   been initialised yet.
   Numerical value: 4

   \def ADAPTYST_ERR_LOG_DIR_CREATE
   Error indicating that Adaptyst couldn't create log directories for
   the current performance analysis session.
   Numerical value: 5

   \def ADAPTYST_ERR_INIT_ONLY
   Error indicating that an API method meant to be called inside
   adaptyst_module_init() only has been attempted to be called
   outside of adaptyst_module_init().
   Numerical value: 6

   \def ADAPTYST_ERR_TIMEOUT
   Error indicating a timeout.
   Numerical value: 7

   \def ADAPTYST_ERR_WORKFLOW_NOT_STARTED
   Error indicating that the workflow hasn't been started.
   Numerical value: 8

   \def ADAPTYST_ERR_TIMESTAMP
   Error indicating that the timestamp couldn't be obtained.
   Numerical value: 9

   \def ADAPTYST_ERR_WORKFLOW_RUNNING
   Error indicating that the workflow is still running.
   Numerical value: 10
*/
#define ADAPTYST_OK 0
#define ADAPTYST_ERR_MODULE_NOT_FOUND 1
#define ADAPTYST_ERR_OUT_OF_MEMORY 2
#define ADAPTYST_ERR_EXCEPTION 3
#define ADAPTYST_ERR_TERMINAL_NOT_INITIALISED 4
#define ADAPTYST_ERR_LOG_DIR_CREATE 5
#define ADAPTYST_ERR_INIT_ONLY 6
#define ADAPTYST_ERR_TIMEOUT 7
#define ADAPTYST_ERR_WORKFLOW_NOT_STARTED 8
#define ADAPTYST_ERR_TIMESTAMP 9
#define ADAPTYST_ERR_WORKFLOW_RUNNING 10

#ifdef __cplusplus
extern "C" {
#endif
  /**
     Enum describing a value type of a module option.
  */
  typedef enum {
    /** C: int */
    INT = 1,
    /** C: const char * */
    STRING = 2,
    /** C: unsigned int */
    UNSIGNED_INT = 3,
    /** C: bool */
    BOOL = 4,
    /** No type. */
    NONE = 0
  } option_type;

  /**
     Struct describing a module option.
  */
  typedef struct {
    /** Value type of a module option. */
    option_type type;

    /**
       Value of a module option. Use "type" to determine what type this
       member should be cast to.
    */
    void *data;

    /**
       Number of elements in case data is an array.
    */
    unsigned int len;
  } option;

  /**
     Enum describing workflow execution types.
  */
  typedef enum {
    /** Workflow is executed as a Linux process. */
    LINUX_PROCESS = 0
  } profile_type;

  /**
     Struct describing information necessary for profiling a workflow.
  */
  typedef struct {
    /** Type of workflow execution. */
    profile_type type;
    /** Union storing proper profiling information. */
    union {
      /**
         If type is LINUX_PROCESS, the PID of a process executing
         a workflow.
      */
      int pid;
    } data;
  } profile_info;

  /**
     Gets the path of a loaded module library
     (e.g. if the library is /A/B/C/libmodule.so, the return
     value is /A/B/C/libmodule.so).

     @param id The module ID (use module_id).

     @return The path of a loaded module.
  */
  const char *adaptyst_get_library_dir(amod_t id);

  /**
     Gets a module option set by a user.

     @param id  The module ID (use module_id).
     @param key The name of an option to obtain.

     @return Pointer to the option struct corresponding to an
             option with the given name or a null pointer if the
             option couldn't be found.
  */
  option *adaptyst_get_option(amod_t id, const char *key);

  /**
     Indicates to Adaptyst that a module error has occurred.

     @param id  The module ID (use module_id).
     @param msg Error message.

     @return Whether the operation has been successful.
  */
  bool adaptyst_set_error(amod_t id, const char *msg);

  /**
     Gets the path to a directory where Adaptyst logs are
     stored.

     @param id The module ID (use module_id).

     @return The path to the Adaptyst log directory or a null
             pointer if the operation hasn't been successful.
  */
  const char *adaptyst_get_log_dir(amod_t id);

  /**
     Sends data to the injection part of the module
     in a workflow.

     @param id  The module ID (use module_id).
     @param buf Data to send.
     @param n   Number of bytes to send.

     @return Whether the operation has been successful.
  */
  bool adaptyst_send_data(amod_t id, char *buf, unsigned int n);

  /**
     Receives data from the injection part of the module
     in a workflow, with no timeout.

     @param id       The module ID (use module_id).
     @param buf      Buffer where received data should be stored.
     @param buf_size Size of the buffer in bytes.
     @param n        Pointer to a variable where the number of received
                     bytes should be stored.

     @return Whether the operation has been successful.
  */
  bool adaptyst_receive_data(amod_t id, char *buf, unsigned int buf_size, int *n);

  /**
     Receives data from the injection part of the module
     in a workflow, with a specific timeout.

     @param id              The module ID (use module_id).
     @param buf             Buffer where received data should be stored.
     @param buf_size        Size of the buffer in bytes.
     @param n               Pointer to a variable where the number of received
                            bytes should be stored.
     @param timeout_seconds Timeout in seconds.

     @return Whether the operation has been successful (it's false with
             adaptyst_get_internal_error_code() returning ADAPTYST_ERR_TIMEOUT
             in case of timeout).
  */
  bool adaptyst_receive_data_timeout(amod_t id, char *buf, unsigned int buf_size,
                                     int *n, long timeout_seconds);

  /**
     Sends a string to the injection part of the module in a workflow.

     The recipient receives strings in the same order they are sent in
     as long as the sender uses adaptyst_send_string() and the receiver uses
     adaptyst_receive_string() or its timeout variant.

     @param id  The module ID (use module_id).
     @param str String to send.

     @return Whether the operation has been successful.
  */
  bool adaptyst_send_string(amod_t id, const char *str);

  /**
     Receives a string from the injection part of the module
     in a workflow, with no timeout.

     If the sender has sent n strings using adaptyst_send_string()
     (where n is a positive integer), adaptyst_receive_string() or
     its timeout variant must be called n times to receive all of these,
     in the order they have been sent in.

     @param id  The module ID (use module_id).
     @param str Pointer to a variable where the received string should
                be stored. If 0 bytes are received, the variable is
                set to a null pointer.

     @return Whether the operation has been successful.
  */
  bool adaptyst_receive_string(amod_t id, const char **str);

  /**
     Receives a string from the injection part of the module
     in a workflow, with a specific timeout.

     If the sender has sent n strings using adaptyst_send_string()
     (where n is a positive integer), adaptyst_receive_string_timeout() or
     its non-timeout variant must be called n times to receive all of these,
     in the order they have been sent in.

     @param id              The module ID (use module_id).
     @param str             Pointer to a variable where the received string
                            should be stored. If 0 bytes are received, the
                            variable is set to a null pointer.
     @param timeout_seconds Timeout in seconds.

     @return Whether the operation has been successful (it's false with
             adaptyst_get_internal_error_code() returning ADAPTYST_ERR_TIMEOUT
             in case of timeout).
  */
  bool adaptyst_receive_string_timeout(amod_t id, const char **str,
                                       long timeout_seconds);

  /**
     Gets the name of a node a module is attached to.

     @param id The module ID (use module_id).

     @return The name of the node or a null pointer if
             the operation hasn't been successful.
  */
  const char *adaptyst_get_node_name(amod_t id);

  /**
     Prints an unformatted message of a given type to Adaptyst logs.

     @param id   The module ID (use module_id).
     @param msg  Message to log.
     @param type Log type (use one of the types declared in log_types).

     @return Whether the operation has been successful.
  */
  bool adaptyst_log(amod_t id, const char *msg, const char *type);

  /**
     Prints a formatted message of a given type to Adaptyst logs, i.e.
     with a specific prefix indicating whether a message is a main one
     or a secondary one *and* whether a message is an error.

     @param id    The module ID (use module_id).
     @param msg   Message to print.
     @param sub   Whether a message is a secondary one (if true, "->" is
                  printed before the message, otherwise "==>" is printed).
     @param error Whether a message is an error message.
     @param type  Log type (use one of the types declared in log_types).

     @return Whether the operation has been successful.
  */
  bool adaptyst_print(amod_t id, const char *msg, bool sub, bool error,
                      const char *type);

  /**
     Gets the path to a directory where all module output files should
     be stored.

     @param id The module ID (use module_id).

     @return The path to the module output directory or a null pointer
             if the operation hasn't been successful.
  */
  const char *adaptyst_get_module_dir(amod_t id);

  /**
     Gets information necessary for profiling a workflow in form
     of a profile_info struct.

     @param id The module ID (use module_id).

     @return Pointer to the profile_info struct with profiling
             information or a null pointer if the operation
             hasn't been successful.
   */
  profile_info *adaptyst_get_profile_info(amod_t id);

  /**
     Sets information necessary for profiling a workflow in form
     of a profile_info struct. This function propagates information
     to all modules within an entity and is useful in case
     a module is in a directing node.

     This method can be called inside adaptyst_module_init() ONLY.
     Otherwise, an error will be thrown.

     @param id   The module ID (use module_id).
     @param info Pointer to a profile_info struct with profiling
                 information to be set.

     @return Whether the operation has been successful.
  */
  bool adaptyst_set_profile_info(amod_t id, profile_info *info);

  /**
     Returns whether a node a module is attached to is a directing node.

     @return Whether the node is a directing node. If the operation hasn't
             been successful, false is returned.
  */
  bool adaptyst_is_directing_node(amod_t id);

  /**
     Sends a notification to Adaptyst that a module is ready to profile.

     @param id The module ID (use module_id).

     @return Whether the operation has been successful.
  */
  bool adaptyst_profile_notify(amod_t id);

  /**
     Waits for a workflow executed by Adaptyst to finish running.

     @param id The module ID (use module_id).

     @return Exit code of the workflow or -1 if the operation hasn't been
             successful.
  */
  int adaptyst_profile_wait(amod_t id);

  /**
     Sends source code paths to Adaptyst for further processing (source code
     packing etc. is handled by Adaptyst, not modules).

     @param id    The module ID (use module_id).
     @param paths Array of source code paths.
     @param n     Number of source code paths.

     @return Whether the operation has been successful.
  */
  bool adaptyst_process_src_paths(amod_t id, const char **paths, int n);

  /**
     Gets the CPU mask, which length is equal to the number of CPU cores
     and where the i-th char (indexing and core numbering from 1) can be either:
     * 'b': the i-th core is for both workflow execution and performance analysis
     * 'p': the i-th core is for performance analysis only
     * 'c': the i-th core is for workflow execution only
     * ' ': the i-th core is not used

     This method can be called inside adaptyst_module_init() ONLY.
     Otherwise, an error will be thrown.

     @param id The module ID (use module_id).

     @return The CPU mask or a null pointer if the operation hasn't been
             successful.
  */
  const char *adaptyst_get_cpu_mask(amod_t id);

  /**
     Gets the path to a temporary directory.

     @param id The module ID (use module_id).

     @return The path to a temporary directory or a null pointer if the
             operation hasn't been successful.
  */
  const char *adaptyst_get_tmp_dir(amod_t id);

  /**
     Gets the path to a directory where Adaptyst local configuration
     files are stored.

     @param id The module ID (use module_id).

     @return The path to the local config directory or a null pointer
             if the operation hasn't been successful.
  */
  const char *adaptyst_get_local_config_dir(amod_t id);

  /**
     Indicates to Adaptyst whether a module will profile a workflow.
     By default, Adaptyst assumes that modules do not profile.

     This method can be called inside adaptyst_module_init() ONLY.
     Otherwise, an error will be thrown.

     @param id           The module ID (use module_id).
     @param will_profile Whether a module will profile a workflow.

     @return Whether the operation has been successful.
  */
  bool adaptyst_set_will_profile(amod_t id, bool will_profile);

  /**
     Checks whether any nodes connected to a node a module is
     attached to (i.e. any nodes with an edge *to* the module
     node) has a specific tag.

     @param id  The module ID (use module_id).
     @param tag Tag to check.

     @return Whether any nodes connected to the node has the tag.
             If the operation hasn't been successful, false is returned.
  */
  bool adaptyst_has_in_tag(amod_t id, const char *tag);

  /**
     Checks whether any nodes a module node is connected to (i.e.
     any nodes with an edge *from* the module node) has a specific tag.

     @param id  The module ID (use module_id).
     @param tag Tag to check.

     @return Whether any nodes the node is connected to has the tag.
             If the operation hasn't been successful, false is returned.
  */
  bool adaptyst_has_out_tag(amod_t id, const char *tag);

  /**
     Gets the error code set by any of the Adaptyst API calls.

     @param id The module ID (use module_id).

     @return Error code or 0 if no error has occurred. See the ADAPTYST_OK
             and ADAPTYST_ERR_* definitions for possible error codes.
  */
  int adaptyst_get_internal_error_code(amod_t id);

  /**
     Gets the error message set by any of the Adaptyst API calls.

     @param id The module ID (use module_id).

     @return Error message or a null pointer if no error has occurred.
  */
  const char *adaptyst_get_internal_error_msg(amod_t id);

  /**
     Gets the current timestamp in nanoseconds in the Unix format.

     @param id The module ID (use module_id).

     @return Timestamp if no errors have occurred, 0 otherwise with
             adaptyst_get_internal_error_code() returning
             ADAPTYST_ERR_TIMESTAMP or ADAPTYST_ERR_MODULE_NOT_FOUND.
  */
  unsigned long long adaptyst_get_timestamp(amod_t id);

  /**
     Gets the timestamp in nanoseconds in the Unix format of when
     the workflow has been started in an entity where the module
     is.

     Specifically, the timestamp refers to the moment when the
     workflow has been started following modules indicating
     their readiness to run performance analysis.

     @param id The module ID (use module_id).

     @return Time of the workflow start in the Unix timestamp
             format if the operation is successful, 0 otherwise with
             adaptyst_get_internal_error_code() returning ADAPTYST_ERR_*.
             If the workflow hasn't been started, the error code
             is ADAPTYST_ERR_WORKFLOW_NOT_STARTED.
  */
  unsigned long long adaptyst_get_workflow_start_time(amod_t id);

  /**
     Gets the timestamp in nanoseconds in the Unix format of when
     the workflow has finished executing in an entity where the module
     is.

     @param id The module ID (use module_id).

     @return Time of the workflow end in the Unix timestamp
             format if the operation is successful, 0 otherwise with
             adaptyst_get_internal_error_code() returning ADAPTYST_ERR_*.
             If the workflow hasn't been started, the error code
             is ADAPTYST_ERR_WORKFLOW_NOT_STARTED. If the workflow is
             still running, the error code is ADAPTYST_ERR_WORKFLOW_RUNNING.
  */
  unsigned long long adaptyst_get_workflow_end_time(amod_t id);

  /**
     Gets whether the workflow is currently running in an entity
     where the module is.

     If the workflow is ready to run but modules haven't indicated
     their readiness to run performance analysis yet, the return
     value is true.

     @param id The module ID (use module_id).

     @return Whether the workflow is running. If the return value
             is false, this means that either the workflow has finished
             executing or hasn't been run.

             WARNING: If the operation hasn't been successful,
             the return value is also false! However, you can determine
             that this is the cause of your problem by checking the
             value of adaptyst_get_internal_error_code().
  */
  bool adaptyst_is_workflow_running(amod_t id);

#ifdef ADAPTYST_MODULE_ENTRYPOINT
  bool adaptyst_module_init(amod_t module_id);
  bool adaptyst_module_process(amod_t module_id, const char *sdfg);
  void adaptyst_module_close(amod_t module_id);

  bool adaptyst_region_start(amod_t module_id, const char *name,
                             const char *part_id, const char *timestamp_str);
  bool adaptyst_region_end(amod_t module_id, const char *name,
                           const char *part_id, const char *timestamp_str);
#endif
#ifdef __cplusplus
}
#endif

#endif
