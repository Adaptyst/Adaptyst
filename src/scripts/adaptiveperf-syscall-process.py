# perf script event handlers, generated by perf script -g python
# Licensed under the terms of the GNU GPL License version 2

# The common_* event handler fields are the most useful fields common to
# all events.  They don't necessarily correspond to the 'common_*' fields
# in the format files.  Those fields not available as handler params can
# be retrieved using Python functions of the form common_*(context).
# See the perf-script-python Documentation for the list of available functions.

from __future__ import print_function
import os
import sys
import re
import json
import subprocess
import socket
from pathlib import Path

sys.path.append(os.environ['PERF_EXEC_PATH'] +
                '/scripts/python/Perf-Trace-Util/lib/Perf/Trace')

event_sock = None
tid_dict = {}


def syscall_callback(stack, ret_value):
    global event_sock

    if int(ret_value) == 0:
        return

    event_sock.sendall((json.dumps([
        '<SYSCALL>', ret_value, list(map(
            lambda x: x['sym']['name'] if 'sym' in x else
            '[' + Path(x['dso']).name + ']' if 'dso' in x else
            f'({x["ip"]:#x})', stack))
        ]) + '\n').encode('utf-8'))


def syscall_tree_callback(syscall_type, comm_name, pid, tid, time,
                          ret_value):
    global event_sock

    event_sock.sendall((json.dumps([
        '<SYSCALL_TREE>', syscall_type, comm_name, pid, tid, time, ret_value])
                        + '\n').encode('utf-8'))


def trace_begin():
    global event_sock

    event_sock = socket.socket()
    event_sock.connect((os.environ['APERF_SERV_ADDR'],
                        int(os.environ['APERF_SERV_PORT'])))

    sock = socket.socket(family=socket.AF_UNIX, type=socket.SOCK_DGRAM)
    sock.sendto(b'\x00', 'start.sock')
    sock.close()


def trace_end():
    event_sock.sendall('<STOP>\n'.encode('utf-8'))
    event_sock.close()


def syscalls__sys_exit_clone3(event_name, context, common_cpu, common_secs,
                              common_nsecs, common_pid, common_comm,
                              common_callchain, __syscall_nr, ret,
                              perf_sample_dict):
    syscall_callback(perf_sample_dict['callchain'], ret)
    syscall_tree_callback('clone3', common_comm, perf_sample_dict['sample']['pid'],
                          perf_sample_dict['sample']['tid'],
                          perf_sample_dict['sample']['time'], ret)


def syscalls__sys_exit_clone(event_name, context, common_cpu, common_secs,
                             common_nsecs, common_pid, common_comm,
                             common_callchain, __syscall_nr, ret,
                             perf_sample_dict):
    syscall_callback(perf_sample_dict['callchain'], ret)
    syscall_tree_callback('clone', common_comm, perf_sample_dict['sample']['pid'],
                          perf_sample_dict['sample']['tid'],
                          perf_sample_dict['sample']['time'], ret)


def syscalls__sys_exit_vfork(event_name, context, common_cpu, common_secs,
                             common_nsecs, common_pid, common_comm,
                             common_callchain, __syscall_nr, ret,
                             perf_sample_dict):
    syscall_callback(perf_sample_dict['callchain'], ret)
    syscall_tree_callback('vfork', common_comm, perf_sample_dict['sample']['pid'],
                          perf_sample_dict['sample']['tid'],
                          perf_sample_dict['sample']['time'], ret)


def syscalls__sys_exit_fork(event_name, context, common_cpu, common_secs,
                            common_nsecs, common_pid, common_comm,
                            common_callchain, __syscall_nr, ret,
                            perf_sample_dict):
    syscall_callback(perf_sample_dict['callchain'], ret)
    syscall_tree_callback('fork', common_comm, perf_sample_dict['sample']['pid'],
                          perf_sample_dict['sample']['tid'],
                          perf_sample_dict['sample']['time'], ret)


def syscalls__sys_exit_execve(event_name, context, common_cpu, common_secs,
                              common_nsecs, common_pid, common_comm,
                              common_callchain, __syscall_nr, ret,
                              perf_sample_dict):
    syscall_tree_callback('execve', common_comm, perf_sample_dict['sample']['pid'],
                          perf_sample_dict['sample']['tid'],
                          perf_sample_dict['sample']['time'], ret)


def syscalls__sys_enter_exit(event_name, context, common_cpu, common_secs,
                             common_nsecs, common_pid, common_comm,
                             common_callchain, __syscall_nr, error_code,
                             perf_sample_dict):
    syscall_tree_callback('exit', common_comm, perf_sample_dict['sample']['pid'],
                          perf_sample_dict['sample']['tid'],
                          perf_sample_dict['sample']['time'], error_code)


def syscalls__sys_enter_exit_group(event_name, context, common_cpu,
                                   common_secs, common_nsecs, common_pid,
                                   common_comm, common_callchain, __syscall_nr,
                                   error_code, perf_sample_dict):
    syscall_tree_callback('exit_group', common_comm, perf_sample_dict['sample']['pid'],
                          perf_sample_dict['sample']['tid'],
                          perf_sample_dict['sample']['time'], error_code)
