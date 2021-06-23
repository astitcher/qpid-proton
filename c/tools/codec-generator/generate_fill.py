from __future__ import annotations

import argparse
import itertools
import os
import re
import sys

from pycparser import c_ast, parse_file


class ParseError(Exception):
    def __init__(self, error):
        super().__init__(error)


indent_size = 4


class ASTNode:
    def __init__(self, function_suffix: str, first: int, types: list[str]):
        self.function_suffix = function_suffix
        self.first_arg = first
        self.types = types

    @staticmethod
    def mk_indent(indent: int):
        return " "*indent*indent_size

    @staticmethod
    def mk_funcall(name: str, args: list[str]):
        return f'{name}({", ".join(args)})'

    def gen_emit_code(self, prefix: list[str], args: list[str], indent: int) -> list[str]:
        return [f'{self.mk_indent(indent)}{self.mk_funcall("emit_"+self.function_suffix, prefix + args[self.first_arg:][:len(self.types)])};']

    def gen_params(self, first_arg: int) -> list[tuple[str, str]]:
        return [(f'arg{i+first_arg}', self.types[i]) for i in range(len(self.types))]

    @staticmethod
    def gen_emit_list(prefix: list[str], args: list[str], node_list: list[ASTNode], indent: int) -> list[str]:
        lines = [n.gen_emit_code(prefix, args, indent) for n in node_list]
        return [i for i in itertools.chain(*lines)]

    @staticmethod
    def gen_params_list(first_arg: int, node_list: list[ASTNode]) -> list[tuple[str, str]]:
        params = []
        arg = first_arg
        for n in node_list:
            r = n.gen_params(arg)
            arg += len(r)
            params.append(r)
        return [i for i in itertools.chain(*params)]


class NullNode(ASTNode):
    def __init__(self, function_suffix: str):
        super().__init__(function_suffix, 0, [])

    def gen_emit_code(self, prefix: list[str], args: list[str], indent: int) -> list[str]:
        return [f'{self.mk_indent(indent)}{self.mk_funcall("emit_"+self.function_suffix, prefix)};']

    def gen_params(self, first_arg: int) -> list[tuple[str, str]]:
        return []


class DescListNode(ASTNode):
    def __init__(self, l: list[ASTNode], first: int):
        self.list: list[ASTNode] = l
        super().__init__('described_list', first, ['uint64_t'])

    def gen_emit_code(self, prefix: list[str], args: list[str], indent: int) -> list[str]:
        return [
            f'{self.mk_indent(indent)}emit_descriptor({", ".join(prefix+args[self.first_arg:][:len(self.types)])});',
            f'{self.mk_indent(indent)}for (bool small_encoding = true; ; small_encoding = false) {{',
            f'{self.mk_indent(indent+1)}pni_compound_context c = '
            f'{self.mk_funcall("emit_list", prefix+["small_encoding"])};',
            f'{self.mk_indent(indent+1)}pni_compound_context compound = c;',
            *self.gen_emit_list(prefix, args, self.list, indent + 1),
            f'{self.mk_indent(indent+1)}{self.mk_funcall("emit_end_list", prefix+["small_encoding"])};',
            f'{self.mk_indent(indent+1)}if (encode_succeeded({", ".join(prefix)})) break;',
            f'{self.mk_indent(indent)}}}',
        ]

    def gen_params(self, first_arg: int) -> list[tuple[str, str]]:
        args = super().gen_params(first_arg)
        return [
            *args,
            *self.gen_params_list(first_arg + len(args), self.list)
        ]

class DescListIgnoreTypeNode(ASTNode):
    def __init__(self, l: list[ASTNode], first: int):
        self.list: list[ASTNode] = l
        super().__init__('described_unknown_list', first, [])

class ArrayNode(ASTNode):
    def __init__(self, l: list[ASTNode], first: int):
        self.list: list[ASTNode] = l
        super().__init__('array', first, ['pn_type_t'])

    def gen_emit_code(self, prefix: list[str], args: list[str], indent: int) -> list[str]:
        return [
            f'{self.mk_indent(indent)}for (bool small_encoding = true; ; small_encoding = false) {{',
            f'{self.mk_indent(indent+1)}pni_compound_context c = '
            f'{self.mk_funcall("emit_array", prefix+["small_encoding"]+args[self.first_arg:][:len(self.types)])};',
            f'{self.mk_indent(indent+1)}pni_compound_context compound = c;',
            *self.gen_emit_list(prefix, args, self.list, indent + 1),
            f'{self.mk_indent(indent+1)}{self.mk_funcall("emit_end_array", prefix+["small_encoding"])};',
            f'{self.mk_indent(indent+1)}if (encode_succeeded({", ".join(prefix)})) break;',
            f'{self.mk_indent(indent)}}}',
        ]

    def gen_params(self, first_arg: int) -> list[tuple[str, str]]:
        args = super().gen_params(first_arg)
        return [
            *args,
            *self.gen_params_list(first_arg + len(args), self.list)
        ]


class OptionNode(ASTNode):
    def __init__(self, i: ASTNode, first: int):
        self.option: ASTNode = i
        super().__init__('option', first, ['bool'])

    def gen_emit_code(self, prefix: list[str], args: list[str], indent: int) -> list[str]:
        return [
            f'{self.mk_indent(indent)}if ({args[self.first_arg]}) {{',
            *self.option.gen_emit_code(prefix, args, indent + 1),
            f'{self.mk_indent(indent)}}} else {{',
            *NullNode("null").gen_emit_code(prefix, args, indent + 1),
            f'{self.mk_indent(indent)}}}'
        ]

    def gen_params(self, first_arg: int) -> list[tuple[str, str]]:
        args = super().gen_params(first_arg)
        return [
            *args,
            *self.option.gen_params(first_arg + len(args))
        ]


class EmptyNode(ASTNode):
    def __init__(self):
        super().__init__('empty_frame', 0, [])

    def gen_emit_code(self, prefix: list[str], args: list[str], indent: int) -> list[str]:
        return []

    def gen_params(self, first_arg: int) -> list[tuple[str, str]]:
        return []


def expect_char(format: str, char: str):
    return format[0] == char, format[1:]


def parse_list(format: str, arg: int) -> tuple[list[ASTNode], str, int]:
    rest = format
    list = []
    narg = arg
    while not rest[0] == ']':
        i, rest, narg = parse_item(rest, narg)
        list.append(i)
    return list, rest, narg


def parse_item(format: str, arg: int) -> tuple[ASTNode, str, int]:
    if len(format) == 0:
        return EmptyNode(), '', arg
    if format.startswith('DL['):
        l, rest, narg = parse_list(format[3:], arg+1)
        b, rest = expect_char(rest, ']')
        if not b:
            raise ParseError(format)
        return DescListNode(l, arg), rest, narg
    elif format.startswith('D.['):
        l, rest, narg = parse_list(format[3:], arg)
        b, rest = expect_char(rest, ']')
        if not b:
            raise ParseError(format)
        return DescListIgnoreTypeNode(l, arg), rest, narg
    elif format.startswith('D?LC'):
        return ASTNode('described_maybe_long_type_list_copy', arg, ['bool', 'uint64_t', 'pn_data_t*']), format[4:], arg + 3
    elif format.startswith('DLC'):
        return ASTNode('described_list_copy', arg, ['uint64_t', 'pn_data_t*']), format[3:], arg + 2
    elif format.startswith('DL.'):
        return ASTNode('described_list_anything', arg, ['uint64_t']), format[3:], arg + 1
    elif format.startswith('D..'):
        return NullNode('described_anything'), format[3:], arg
    elif format.startswith('@T['):
        l, rest, narg = parse_list(format[3:], arg+1)
        b, rest = expect_char(rest, ']')
        if not b:
            raise ParseError(format)
        return ArrayNode(l, arg), rest, narg
    elif format.startswith('?'):
        i, rest, narg = parse_item(format[1:], arg+1)
        return OptionNode(i, arg), rest, narg
    elif format.startswith('*s'):
        return ASTNode('counted_symbols', arg, ['size_t', 'char**']), format[2:], arg+2
    elif format.startswith('.'):
        return NullNode('anything'), format[1:], arg
    elif format.startswith('s'):
        return ASTNode('symbol', arg, ['const char*']), format[1:], arg+1
    elif format.startswith('S'):
        return ASTNode('string', arg, ['const char*']), format[1:], arg+1
    elif format.startswith('C'):
        return ASTNode('copy', arg, ['pn_data_t*']), format[1:], arg+1
    elif format.startswith('I'):
        return ASTNode('uint', arg, ['uint32_t']), format[1:], arg+1
    elif format.startswith('H'):
        return ASTNode('ushort', arg, ['uint16_t']), format[1:], arg+1
    elif format.startswith('n'):
        return NullNode('null'), format[1:], arg
    elif format.startswith('M'):
        return ASTNode('multiple', arg, ['pn_data_t*']), format[1:], arg+1
    elif format.startswith('o'):
        return ASTNode('bool', arg, ['bool']), format[1:], arg+1
    elif format.startswith('B'):
        return ASTNode('ubyte', arg, ['uint8_t']), format[1:], arg+1
    elif format.startswith('L'):
        return ASTNode('ulong', arg, ['uint64_t']), format[1:], arg+1
    elif format.startswith('t'):
        return ASTNode('timestamp', arg, ['pn_timestamp_t']), format[1:], arg + 1
    elif format.startswith('z'):
        return ASTNode('binaryornull', arg, ['size_t', 'const char*']), format[1:], arg+2
    elif format.startswith('Z'):
        return ASTNode('binarynonull', arg, ['size_t', 'const char*']), format[1:], arg+2
    else:
        raise ParseError(format)


def expect_quoted_string(arg: str) -> str:
    b, rest = expect_char(arg, '"')
    if not b:
        raise ParseError(arg)
    if rest[0] == '"':
        return ""

    b, rest = expect_char(arg[-1:], '"')
    if not b:
        raise ParseError(rest)
    return arg[1:-1]


# Need to translate '@[]*?.' to legal identifier characters
# These will be fairly arbitrary and just need to avoid already used codes
def make_legal_identifier(s: str) -> str:
    subs = {'@': 'A', '[': 'E', ']': 'e', '{': 'F', '}': 'f', '*': 'j', '.': 'q', '?': 'Q'}
    r = ''
    for c in s:
        if c in subs:
            r += subs[c]
        else:
            r += c
    return r


def emit_function(prefix: str, fill_spec: str, prefix_args: list[tuple[str, str]]) -> tuple[str, list[str]]:
    p, _, _ = parse_item(fill_spec, 0)
    params = p.gen_params(0)
    function_name = prefix + '_' + make_legal_identifier(fill_spec)
    param_list = ', '.join([t+' '+arg for arg, t in prefix_args+params])
    function_spec = f'pn_bytes_t {function_name}({param_list})'

    bytes_args = [('bytes', 'char*'), ('size', 'size_t')]
    bytes_function_name = prefix + '_bytes_' + make_legal_identifier(fill_spec)
    bytes_param_list = ', '.join([t+' '+arg for arg, t in bytes_args+params])
    bytes_function_spec = f'size_t {bytes_function_name}({bytes_param_list})'

    inner_function_name = prefix + '_inner_' + make_legal_identifier(fill_spec)
    inner_param_list = [('emitter', 'pni_emitter_t*')]+params
    inner_params = ', '.join([t+' '+arg for arg, t in inner_param_list])
    inner_function_spec = f'bool {inner_function_name}({inner_params})'

    function_decl = f'{function_spec};\n{bytes_function_spec};'

    prefix = [a for (a, _) in prefix_args]
    args = [a for (a, _) in params]
    if type(p) is EmptyNode:
        function_defn = [
            f'{function_spec}',
            '{',
            f'{p.mk_indent(1)}pni_emitter_t emitter = make_emitter({", ".join(prefix)});',
            f'{p.mk_indent(1)}return make_return(emitter);',
            '}',
            ''
        ]
    else:
        function_defn = [
            f'{inner_function_spec}',
            '{',
            f'{p.mk_indent(1)}pni_compound_context compound = make_compound();',
            *p.gen_emit_code(['emitter', '&compound'], args, 1),
            f'{p.mk_indent(1)}return resize_required(emitter);',
            '}',
            '',
            f'{function_spec}',
            '{',
            f'{p.mk_indent(1)}do {{',
            f'{p.mk_indent(2)}pni_emitter_t emitter = make_emitter_from_buffer({", ".join(prefix)});',
            f'{p.mk_indent(2)}if ({p.mk_funcall(inner_function_name, ["&emitter", *args])}) {{',
            f'{p.mk_indent(3)}{p.mk_funcall("size_buffer_to_emitter", [*prefix, "&emitter"])};',
            f'{p.mk_indent(3)}continue;',
            f'{p.mk_indent(2)}}}',
            f'{p.mk_indent(2)}return make_bytes_from_emitter(emitter);',
            f'{p.mk_indent(1)}}} while (true);',
            f'{p.mk_indent(1)}/*Unreachable*/',
            '}',
            ''
            f'{bytes_function_spec}',
            '{',
            f'{p.mk_indent(1)}pni_emitter_t emitter = make_emitter_from_bytes((pn_rwbytes_t){{.size=size, .start=bytes}});',
            f'{p.mk_indent(1)}{p.mk_funcall(inner_function_name, ["&emitter", *args])};',
            f'{p.mk_indent(1)}return make_bytes_from_emitter(emitter).size;',
            '}',
            ''
        ]
    return function_decl, function_defn


def find_function_calls(file: str, name: str, includes: list[str], defines: list[str]) -> list[list[str]]:
    class FillFinder(c_ast.NodeVisitor):
        def __init__(self):
            self._name = name
            self.result = []

        def visit_FuncCall(self, node):
            if node.name.name == self._name:
                r = []
                for e in node.args.exprs:
                    r.append(e)
                self.result.append(r)

    include_args = [f'-I{d}' for d in includes]
    define_args = [f'-D{d}' for d in defines]
    ast = parse_file(file, use_cpp=True,
                     cpp_args=[
                         *include_args,
                         *define_args
                     ])
    ff = FillFinder()
    ff.visit(ast)
    return ff.result


prefix_header = """
#include "proton/codec.h"
#include "buffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

"""


def output_header(decls, file=None):
    # Output declarations
    print(prefix_header, file=file)
    for fill_spec, decl in decls.items():
        print(
            f'/* {fill_spec} */\n'
            f'{decl}',
            file=file
        )


prefix_implementation = """
#include "core/emitters.h"
"""


def output_implementation(defns, header=None, file=None):
    # Output implementations
    if header:
        print(f'#include "{header}"', file=file)

    print(prefix_implementation, file=file)
    for fill_spec, defn in defns.items():
        printable_defn = '\n'.join(defn)
        print(
            f'/* {fill_spec} */\n'
            f'{printable_defn}',
            file=file
        )


def emit(c_defines, c_includes, decl_filename, impl_filename, pn_source):
    amqp_calls = find_function_calls(os.path.join(pn_source, 'c/src/core/transport.c'), 'pn_fill_performative',
                                     c_includes, c_defines)
    sasl_calls = find_function_calls(os.path.join(pn_source, 'c/src/sasl/sasl.c'), 'pn_fill_performative',
                                     c_includes, c_defines)
    message_calls = find_function_calls(os.path.join(pn_source, 'c/src/core/message.c'), 'pn_data_fill',
                                        c_includes, c_defines)

    fill_spec_args = [ c[1] for c in amqp_calls ]
    fill_spec_args += [ c[1] for c in sasl_calls ]
    fill_spec_args += [ c[1] for c in message_calls ]
    fill_specs = {expect_quoted_string(e.value) for e in fill_spec_args if type(e) is c_ast.Constant and e.type=='string'}

    decls = {}
    defns = {}
    for fill_spec in fill_specs:
        decl, defn = emit_function('pn_amqp_encode', fill_spec, [('buffer', 'pn_buffer_t*')])
        decls[fill_spec] = decl
        defns[fill_spec] = defn
    if decl_filename and impl_filename:
        with open(decl_filename, 'w') as dfile:
            output_header(decls, file=dfile)
        with open(impl_filename, 'w') as ifile:
            output_implementation(defns, header=os.path.basename(decl_filename), file=ifile)
    else:
        output_header(decls)
        output_implementation(defns)


def consume(c_defines, c_includes, decl_filename, impl_filename, pn_source):
    calls = find_function_calls(os.path.join(pn_source, 'c/src/core/transport.c'),
                                'pn_data_scan',
                                c_includes, c_defines)
    scan_spec_args = (c[1] for c in calls)
    # Only try to generate code for constant strings
    scan_specs = {expect_quoted_string(e.value) for e in scan_spec_args if type(e) is c_ast.Constant and e.type=='string'}
    for scan_spec in scan_specs:
        print(scan_spec)
        print(parse_item(scan_spec, 0))


def main():
    argparser = argparse.ArgumentParser(description='Parse C looking for data scan/fill function calls')
    argparser.add_argument('-s', '--source', help='proton source directory', type=str, required=True)
    argparser.add_argument('-b', '--build', help='proton build directory', type=str, required=True)
    argparser.add_argument('-o', '--output-base', help='basename for output code', type=str)
    group = argparser.add_mutually_exclusive_group(required=True)
    group.add_argument('-e', '--emit', help='generate code to emit amqp', action='store_true')
    group.add_argument('-c', '--consume', help='generate code to consume amqp', action='store_true')

    args = argparser.parse_args()

    if args.output_base:
        decl_filename = args.output_base + '.h'
        impl_filename = args.output_base + '.c'
    else:
        decl_filename = None
        impl_filename = None

    pn_source = args.source
    pn_build = args.build

    c_defines = ['GEN_FILL',
                 'NDEBUG',
                 '__attribute__(X)=',
                 '__asm__(X)=',
                 '__inline=',
                 '__extension__=',
                 '__restrict=',
                 '__builtin_va_list=int']

    c_includes = [f'{os.path.join(pn_build, "c/include")}',
                  f'{os.path.join(pn_build, "c/src")}']

    if args.emit:
        emit(c_defines, c_includes, decl_filename, impl_filename, pn_source)
    elif args.consume:
        consume(c_defines, c_includes, decl_filename, impl_filename, pn_source)

    # cg = pycparser.c_generator
    # generator = cg.CGenerator()
    # print(generator.visit(ast))


if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw?|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
