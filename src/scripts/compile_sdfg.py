from dace.sdfg import sdfg
import json

def compile(sdfg_str: str, out_path: str):
    graph = sdfg.SDFG.from_json(json.loads(sdfg_str))
    graph.compile(output_file=out_path,
                  return_program_handle=False)
