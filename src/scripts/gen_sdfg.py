from dace.sdfg import sdfg, state
from dace import dtypes
import json
import yaml


def gen_sdfg_from_cmd(cmd: list):
    graph = sdfg.SDFG(name='AdaptystRootSDFG')
    state = graph.add_state('initial', True)
    main_cmd = cmd[0].replace('"', r'\"')
    state.add_tasklet('run_cmd', [], [], """
int forked = fork();
if (forked == 0) {
  char *const argv[] = { """ +
                      ', '.join(list(map(lambda x:
                                         f'const_cast<char *>("{x}")',
                                         cmd[1:])) + ['NULL']) + ' };' + """
  execvp(""" + f'"{main_cmd}", argv);' + """
  std::exit(errno);
} else {
  waitpid(forked, NULL, NULL);
}
""", language=dtypes.Language.CPP,
                      code_global="""
#include <unistd.h>
#include <sys/wait.h>
""")
    return json.dumps(graph.to_json())


def gen_sdfg_from_yml(yml_path: str):
    print(('gen_sdfg_from_yml', yml_path))
    return 'yml'
