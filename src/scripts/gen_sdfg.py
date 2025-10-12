from dace.sdfg import sdfg, state
from dace import dtypes
from dace.memlet import Memlet
import dace
import json
import yaml


def gen_sdfg_from_cmd(cmd: list):
    graph = sdfg.SDFG(name='AdaptystRootSDFG')
    state = graph.add_state('initial', True)
    for i in range(len(cmd)):
        cmd[i] = cmd[i].replace('"', r'\"')
    main_cmd = cmd[0]
    node = state.add_tasklet('run_cmd', [], [], """
int forked = fork();
if (forked == 0) {
  char *const argv[] = { """ +
  ', '.join(list(map(lambda x:
                     f'const_cast<char *>("{x}")',
                     cmd)) + ['NULL']) + ' };' + """
  execvp(""" + f'"{main_cmd}", argv);' + """
  std::exit(errno);
} else {
  int status;
  int result = waitpid(forked, &status, NULL);
  if (result != forked) {
    code = 104;
  } else if (WIFEXITED(status)) {
    code = WEXITSTATUS(status);
  } else {
    code = 210;
  }
}
""", language=dtypes.Language.CPP,
                      code_global="""
#include <unistd.h>
#include <sys/wait.h>
""")
    node.add_out_connector('code', dtype=dace.int32)
    return_node = state.add_access('exit_code')
    graph.add_array('exit_code', shape=(1,), dtype=dace.int32)
    return_node.data = 'exit_code'
    state.add_edge(node, 'code', return_node, 'code',
                   Memlet(data='exit_code'))
    return json.dumps(graph.to_json())


def gen_sdfg_from_yml(yml_path: str):
    print(('gen_sdfg_from_yml', yml_path))
    return 'yml'
