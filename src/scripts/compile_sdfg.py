# SPDX-FileCopyrightText: 2025 CERN
# SPDX-License-Identifier: GPL-3.0-or-later

from dace import Config
from dace.sdfg import sdfg
import json
import platform

def compile(sdfg_str: str, out_path: str):
    machine = platform.machine()

    if machine.startswith('rv32') or \
       machine.startswith('rv64') or \
       machine.startswith('riscv'):
        flags = Config.get('compiler', 'cpu', 'args')
        flags = flags.replace('-march=native', '')
        Config.set('compiler', 'cpu', 'args', value=flags)

    graph = sdfg.SDFG.from_json(json.loads(sdfg_str))
    graph.compile(output_file=out_path,
                  return_program_handle=False)
