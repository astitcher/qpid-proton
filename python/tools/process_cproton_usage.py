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
            if x.id.startswith('PN_') or x.id.startswith('pn_'):
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

class DeclVisitor(c_ast.NodeVisitor):
    def __init__(self):
        self.decls = {}
        self.enums = {}
        self.typedefs = {}
        super(DeclVisitor, self).__init__()

    def visit_Decl(self, node):
        self.decls[node.name] = node

    def visit_Enum(self, node):
        #  Enumerators are below EnumeratorList
        for e in node.values.enumerators:
            self.enums[e.name] = node

    def visit_Typedef(self, node):
        self.typedefs[node.name] = node
        if isinstance(node.type.type, c_ast.Enum):
            for e in node.type.type.values.enumerators:
                self.enums[e.name] = node

class TypeDeclVisitor(c_ast.NodeVisitor):
    def __init__(self):
        self.used_types = set()
        super(TypeDeclVisitor, self).__init__()

    def visit_IdentifierType(self, node):
        self.used_types.add(node.names[0])

def parse_include_decls(file, include):
    ast = parse_include(file, include)
    v = DeclVisitor()
    v.visit(ast)
    return v.decls, v.enums, v.typedefs

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

    #print(syms)
    symset = set(syms.keys())

    if i:
        decls, enums, typedefs = parse_include_decls(i, p)
        #print(decls)
        #print(enums)
        #print(typedefs)

        generator = c_generator.CGenerator()
        used_decls = set()
        used_enums = set()
        used_types = set()
        nondecls = set()
        printed = set()
        force_print =  [
            '__ssize_t',
            '__int8_t',
            '__int16_t',
            '__int32_t',
            '__int64_t',
            '__uint8_t',
            '__uint16_t',
            '__uint32_t',
            '__uint64_t',
            'size_t',
            'ssize_t',
            'int8_t',
            'int16_t',
            'int32_t',
            'int64_t',
            'uint8_t',
            'uint16_t',
            'uint32_t',
            'uint64_t'
        ]

        for s in symset:
            if s in decls:
                used_decls.add(s)
                v = TypeDeclVisitor()
                v.visit(decls[s])
                used_types |= v.used_types
            elif s in enums:
                used_enums.add(s)
            else:
                nondecls.add(s)

        for t in force_print + sorted(used_types):
            try:
                tp = typedefs[t]
                if tp not in printed:
                    print('%s;' % generator.visit(tp))
                    printed.add(tp)
            except KeyError:
                pass

        for s in sorted(used_enums):
            e = enums[s]
            if e not in printed:
                print('%s;' % generator.visit(e))
                printed.add(e)

        for s in sorted(used_decls):
            print('%s;' % generator.visit(decls[s]))

        print("// Couldn't find definitions for the following symbols are they #defined?")
        for  s in sorted(nondecls):
            print('// %s' % s)


if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw?|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
