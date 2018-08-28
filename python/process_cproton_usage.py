from __future__ import absolute_import
from __future__ import print_function

import yaml
import ast
import os

def read_imports_yaml(filename: str):
    """
    Read YAML file derived from imports
    :param filename: input file
    :return: sorted list of unique symbols
    """
    y = yaml.safe_load(open(filename))
    s = {x for i in y for x in y[i]}
    return sorted(x for x in s)

def parse_usage(filename: str):
    """
    Parse python for proton symbols
    :param filename: python file to parse
    :return: generator yielding tuple of symbol and location
    """
    with open(filename) as f:
        s = f.read()
    p = ast.parse(s)
    for x in ast.walk(p):
        if isinstance(x, ast.Name):
            if x.id.startswith('PN') or x.id.startswith('pn'):
                yield (x.id, (filename, x.lineno, x.col_offset))


def parse_all(dir: str):
    """
    Parse an entire directory of python files for proton symbols
    :param dir:
    :return:
    """
    for d,_,lf in os.walk(dir):
        for f in lf:
            if f.endswith('.py'):
                for x in parse_usage(os.path.join(d,f)):
                    yield x


