#!/usr/bin/env python

import ast
import difflib
import tempfile
import unittest
from pathlib import Path

from tadashi.apps import Simple
from tadashi.tadashilib import Scops, Transformation

HEADER = "/// TRANSFORMATION: "
COMMENT = "///"


def foobar(app):
    target_code = []
    with open(app.source) as file:
        for line in file:
            if line.startswith(HEADER):
                transform_str = line.replace(HEADER, "")
            elif line.startswith(COMMENT):
                stripped_line = line.strip().replace(COMMENT, "")
                if len(line) > len(COMMENT):
                    stripped_line = stripped_line[1:]
                target_code.append(stripped_line)
    transform = list(ast.literal_eval(transform_str))
    transform_and_args = [Transformation(transform[0])] + transform[1:]
    return transform_and_args, target_code


class TestCtadashi(unittest.TestCase):
    def get_filtered_code(self, scops):
        with tempfile.TemporaryDirectory() as tmpdir:
            outfile = Path(tmpdir) / self._testMethodName
            outfile_bytes = str(outfile).encode()
            scops.ctadashi.generate_code(scops.source_path_bytes, outfile_bytes)
            generated_code = Path(outfile_bytes.decode()).read_text().split("\n")
        return [x for x in generated_code if not x.startswith(COMMENT)]

    def test_lit(self):
        app = Simple(source=Path(__file__).parent.parent / "threeloop.c")
        transform_with_args, target_code = foobar(app)

        # transform
        scop_idx = 0
        node_idx = 2
        scops = Scops(app)
        scop = scops[scop_idx]  # select_scop()
        node = scop.schedule_tree[node_idx]  # model.select_node(scop)
        node.transform(*transform_with_args)

        filtered_code = self.get_filtered_code(scops)
        diff = difflib.unified_diff(filtered_code, target_code)
        diff_str = "\n".join(diff)
        if diff_str:
            print(diff_str)
        self.assertFalse(diff_str)


if __name__ == "__main__":
    print("Hello")
