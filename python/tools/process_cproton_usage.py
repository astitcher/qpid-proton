from __future__ import absolute_import
from __future__ import print_function

import argparse
import ast
import os
from pycparser import parse_file, c_ast, c_generator
import re
import sys
import yaml

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

def parse_include(file: str, include: str):
    return parse_file(file, use_cpp=True, cpp_args=f'-I{os.path.abspath(include)}')

class FuncDefVisitor(c_ast.NodeVisitor):
    def __init__(self, syms):
        self.syms = syms
        self.generator = c_generator.CGenerator()
        super(FuncDefVisitor, self).__init__()

    def visit_Decl(self, node):
        if isinstance(node.type, c_ast.FuncDecl) and node.name in self.syms:
            print(self.generator.visit(node))

class DeclVisitor(c_ast.NodeVisitor):
    def __init__(self):
        self.decls = {}
        super(DeclVisitor, self).__init__()

    def visit_Decl(self, node):
        self.decls[node.name] = node

def parse_include_decls(file, include):
    ast = parse_include(file, include)
    v = DeclVisitor()
    v.visit(ast)
    return v.decls

def main():
    argparser = argparse.ArgumentParser(description='Parse python looking for proton "pn_" & "PN_" symbols')
    argparser.add_argument('dir', help='Root directory to parse', type=str)
    argparser.add_argument('-i', '--include', help='root include file (needs to include all other include files)', type=str)
    argparser.add_argument('-p', '--proton-includes', help='include directory for proton includes', type=str)
    args = argparser.parse_args()
    d = args.dir
    i = args.include
    p = args.proton_includes
    syms = {}
    for sym, (file, line, char) in parse_all(d):
        file = os.path.abspath(file)
        finfo = syms.get(sym, {})
        linfo = finfo.get(file, [])
        linfo.append((line, char))
        finfo.update({file: linfo})
        syms.update({sym: finfo})

    syms = set(syms.keys())

    if i:
        decls = parse_include_decls(i, p)
        #print(decls)

        generator = c_generator.CGenerator()
        nondecls = []
        for s in syms:
            try:
                print(generator.visit(decls[s]))
            except KeyError:
                nondecls.append(s)
        print(sorted(nondecls))

        #ast = parse_include(i, p)
        #v = FuncDefVisitor(syms)
        #v.visit(ast)

if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw?|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
