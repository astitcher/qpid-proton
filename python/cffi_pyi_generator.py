#!/usr/bin/env python3
"""
Generate .pyi stub files from C declarations using pycparser.

This script parses C declarations and converts them to Python type annotations
suitable for use with CFFI libraries.

Usage:
    python cffi_to_pyi.py declarations.c output.pyi
    or
    python cffi_to_pyi.py --inline "int foo(int x);" output.pyi

Requirements:
    pip install pycparser
"""

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Union

from pycparser import c_ast
from pycparser.c_parser import CParser
from cffi import cparser


class CFFIToPyiConverter:
    """Convert C declarations to Python .pyi stub format."""

    c_to_python_types = {
        # Integer types
        'char': 'int',
        'signed char': 'int',
        'unsigned char': 'int',
        'short': 'int',
        'short int': 'int',
        'signed short': 'int',
        'signed short int': 'int',
        'unsigned short': 'int',
        'unsigned short int': 'int',
        'int': 'int',
        'signed': 'int',
        'signed int': 'int',
        'unsigned': 'int',
        'unsigned int': 'int',
        'long': 'int',
        'long int': 'int',
        'signed long': 'int',
        'signed long int': 'int',
        'unsigned long': 'int',
        'unsigned long int': 'int',
        'long long': 'int',
        'long long int': 'int',
        'signed long long': 'int',
        'signed long long int': 'int',
        'unsigned long long': 'int',
        'unsigned long long int': 'int',

        # Floating point types
        'float': 'float',
        'double': 'float',
        'long double': 'float',

        # Other types
        'void': 'None',
        'size_t': 'int',
        'ssize_t': 'int',
        'ptrdiff_t': 'int',
        'intptr_t': 'int',
        'uintptr_t': 'int',
        '_Bool': 'bool',
    }

    typedefs_to_ignore = set(['uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
                              'int8_t', 'int16_t', 'int32_t', 'int64_t',
                              'size_t', 'ssize_t'])

    def __init__(self):
        """Initialize the converter with empty type mappings."""
        self.structs: Dict[str, List[Tuple[str, str]]] = {}
        self.unions: Dict[str, List[Tuple[str, str]]] = {}
        self.enums: Dict[str, List[str]] = {}
        self.typedefs: Dict[str, str] = {}
        self.functions: List[Tuple[str, str, List[Tuple[str, str]]]] = []
        self.constants: List[Tuple[str, str]] = []

    def parse_c_code(self, c_code: str) -> None:
        """Parse C code and extract declarations."""
        # Preprocess the code to handle common CFFI patterns
        c_code, macros = self._preprocess_c_code(c_code)

        try:
            parser = CParser()
            ast = parser.parse(c_code, filename='<input>')
            self._visit_ast(ast)
        except Exception as e:
            print(f"Warning: Failed to parse some C code: {e}", file=sys.stderr)
            raise e

    def _preprocess_c_code(self, c_code: str) -> str:
        """Preprocess C code to make it more parseable."""
        # Add fake includes for common types
        fake_includes = """
        typedef unsigned char uint8_t;
        typedef unsigned short uint16_t;
        typedef unsigned int uint32_t;
        typedef unsigned long uint64_t;
        typedef signed char int8_t;
        typedef signed short int16_t;
        typedef signed int int32_t;
        typedef signed long int64_t;
        typedef unsigned long size_t;
        typedef long ssize_t;
        """
        # Use the CFFI preprocessor to handle macros and includes
        return cparser._preprocess(fake_includes + "\n" + c_code)

    def _visit_ast(self, node: c_ast.Node) -> None:
        """Visit AST nodes and extract type information."""
        if isinstance(node, c_ast.FileAST):
            for child in node:
                self._visit_ast(child)

        elif isinstance(node, c_ast.FuncDef):
            self._process_function(node)

        elif isinstance(node, c_ast.Decl):
            if isinstance(node.type, c_ast.FuncDecl):
                self._process_function_decl(node)
            elif isinstance(node.type, (c_ast.Struct, c_ast.Union)):
                self._process_struct_union(node)
            elif isinstance(node.type, c_ast.Enum):
                self._process_enum(node)
            else:
                self._process_variable_decl(node)

        elif isinstance(node, c_ast.Typedef):
            self._process_typedef(node)

        elif isinstance(node, (c_ast.Struct, c_ast.Union)):
            self._process_struct_union_standalone(node)

        elif isinstance(node, c_ast.Enum):
            self._process_enum_standalone(node)

        # Recursively visit children
        for child in node:
            self._visit_ast(child)

    def _process_function(self, node: c_ast.FuncDef) -> None:
        """Process function definition."""
        func_name = node.decl.name
        return_type = self._get_type_annotation(node.decl.type.type)
        params = self._get_function_params(node.decl.type)
        self.functions.append((func_name, return_type, params))

    def _process_function_decl(self, node: c_ast.Decl) -> None:
        """Process function declaration."""
        func_name = node.name
        return_type = self._get_type_annotation(node.type.type)
        params = self._get_function_params(node.type)
        if (func_name, return_type, params) in self.functions:
            return  # Avoid duplicates
            # raise ValueError(f"Duplicate function declaration: {func_name}")
        self.functions.append((func_name, return_type, params))

    def _process_struct_union(self, node: c_ast.Decl) -> None:
        """Process struct/union declaration."""
        if isinstance(node.type, c_ast.Struct):
            self._process_struct_union_standalone(node.type)
        elif isinstance(node.type, c_ast.Union):
            self._process_struct_union_standalone(node.type)

    def _process_struct_union_standalone(self, node: Union[c_ast.Struct, c_ast.Union]) -> None:
        """Process standalone struct/union."""
        if not node.name:
            return

        fields = []
        if node.decls:
            for decl in node.decls:
                if isinstance(decl, c_ast.Decl):
                    field_name = decl.name
                    field_type = self._get_type_annotation(decl.type)
                    fields.append((field_name, field_type))

        if isinstance(node, c_ast.Struct):
            self.structs[node.name] = fields
        else:  # Union
            self.unions[node.name] = fields

    def _process_enum(self, node: c_ast.Decl) -> None:
        """Process enum declaration."""
        if isinstance(node.type, c_ast.Enum):
            self._process_enum_standalone(node.type)

    def _process_enum_standalone(self, node: c_ast.Enum) -> None:
        """Process standalone enum."""
        if not node.name or not node.values:
            return

        values = []
        next_value = 0
        for enumerator in node.values.enumerators:
            if enumerator.value is not None:
                if isinstance(enumerator.value, c_ast.Constant):
                    next_value = int(enumerator.value.value)
                elif isinstance(enumerator.value, c_ast.UnaryOp) and enumerator.value.op == '-':
                    # Handle negative constants
                    next_value = -int(enumerator.value.expr.value)
            # Store the enumerator name and its value
            values.append((enumerator.name, next_value))
            next_value += 1

        self.enums[node.name] = values

    def _process_typedef(self, node: c_ast.Typedef) -> None:
        """Process typedef declaration."""
        type_name = node.name
        underlying_type = self._get_type_annotation(node.type)
        self.typedefs[type_name] = underlying_type

    def _process_variable_decl(self, node: c_ast.Decl) -> None:
        """Process variable declaration (constants)."""
        if node.name and node.init:
            var_type = self._get_type_annotation(node.type)
            self.constants.append((node.name, var_type))

    def _get_function_params(self, func_type: c_ast.FuncDecl) -> List[Tuple[str, str]]:
        """Get function parameters."""
        params = []
        if func_type.args:
            for param in func_type.args.params:
                if isinstance(param, c_ast.Decl):
                    param_name = param.name or f"param_{len(params)}"
                    param_type = self._get_type_annotation(param.type)
                    params.append((param_name, param_type))
                elif isinstance(param, c_ast.EllipsisParam):
                    params.append(("args", "*args: Any"))
        return params

    def _get_type_annotation(self, type_node: c_ast.Node) -> str:
        """Convert C type to Python type annotation."""
        if isinstance(type_node, c_ast.IdentifierType):
            c_type = ' '.join(type_node.names)
            # Check if it's a known struct/union/enum first
            if c_type in self.structs or c_type in self.unions or c_type in self.enums:
                return c_type
            # Check if it's a typedef and resolve to its underlying c type
            while c_type in self.typedefs:
                new_c_type = self.typedefs[c_type]
                if new_c_type == c_type:  # No change, stop resolving
                    break
                c_type = new_c_type
            # Otherwise use built-in type mapping
            return self.c_to_python_types.get(c_type, c_type)

        elif isinstance(type_node, c_ast.PtrDecl):
            # Pointer types
            pointed_type = self._get_type_annotation(type_node.type)
            if pointed_type == 'int' and isinstance(type_node.type, c_ast.TypeDecl) \
               and isinstance(type_node.type.type, c_ast.IdentifierType):
                base_type = ' '.join(type_node.type.type.names)
                if base_type == 'char':
                    return 'bytes'  # char* is typically bytes in Python
                elif base_type == 'void':
                    return 'Any'    # void* is Any
            # For struct/union pointers, return the type name (CFFI handles the pointer aspect)
            if pointed_type != 'Any' and not pointed_type.startswith("'"):
                return pointed_type
            return 'Any'  # Generic pointer

        elif isinstance(type_node, c_ast.ArrayDecl):
            # Array types
            return 'Any'  # Arrays are typically handled as pointers in CFFI

        elif isinstance(type_node, c_ast.FuncDecl):
            # Function pointer
            return 'Any'  # Function pointers are complex

        elif isinstance(type_node, c_ast.Struct):
            return type_node.name if type_node.name else 'Any'

        elif isinstance(type_node, c_ast.Union):
            return type_node.name if type_node.name else 'Any'

        elif isinstance(type_node, c_ast.Enum):
            return 'int'  # Enums are integers in Python

        elif isinstance(type_node, c_ast.TypeDecl):
            return self._get_type_annotation(type_node.type)

        return 'Any'

    def generate_pyi(self) -> str:
        """Generate .pyi stub file content."""
        lines = []

        # Header
        lines.append('"""')
        lines.append('CFFI-generated stub file.')
        lines.append('This file was automatically generated from C declarations.')
        lines.append('"""')
        lines.append('')
        lines.append('from typing import Any, Literal, Optional, Union')
        lines.append('from cffi import FFI')

        # Enums (define first as they might be used by other types)
        if self.enums:
            lines.append('')
            lines.append('')
            lines.append('# Enums')
            for enum_name, values in self.enums.items():
                for name, value in values:
                    lines.append(f'{name}: Literal[{value}]  # {enum_name}')

        # Structs (define before typedefs that might reference them)
        if self.structs:
            lines.append('')
            lines.append('')
            lines.append('# Structures')
            for struct_name, fields in self.structs.items():
                if fields:
                    lines.append(f'class {struct_name}:')
                    for field_name, field_type in fields:
                        lines.append(f'    {field_name}: {field_type}')
                    lines.append('')
                    lines.append('')
                else:
                    lines.append(f'class {struct_name}: ...')

        # Unions
        if self.unions:
            lines.append('')
            lines.append('')
            lines.append('# Unions')
            for union_name, fields in self.unions.items():
                lines.append(f'class {union_name}:')
                if fields:
                    for field_name, field_type in fields:
                        lines.append(f'    {field_name}: {field_type}')
                else:
                    lines.append('    pass')
                lines.append('')
                lines.append('')

        # Typedefs (only for non-struct/union types)
        typedef_aliases = {}
        for typedef_name, underlying_type in self.typedefs.items():
            # Don't create aliases for structs/unions as they're already defined as classes
            if typedef_name not in self.structs and typedef_name not in self.unions \
               and typedef_name not in self.typedefs_to_ignore:
                typedef_aliases[typedef_name] = underlying_type

        if typedef_aliases:
            lines.append('')
            lines.append('')
            lines.append('# Type aliases')
            for typedef_name, underlying_type in typedef_aliases.items():
                lines.append(f'{typedef_name} = {underlying_type}')

        # Constants
        if self.constants:
            lines.append('')
            lines.append('')
            lines.append('# Constants')
            for const_name, const_type in self.constants:
                lines.append(f'{const_name}: {const_type}')

        # Functions
        if self.functions:
            lines.append('')
            lines.append('')
            lines.append('# Functions')
            for func_name, return_type, params in self.functions:
                param_strs = []
                for param_name, param_type in params:
                    if param_type.startswith('*args'):
                        param_strs.append(param_type)
                    else:
                        param_strs.append(f'{param_name}: {param_type}')

                param_list = ', '.join(param_strs)
                lines.append(f'def {func_name}({param_list}) -> {return_type}: ...')

        # CFFI objects
        lines.append('')
        lines.append('')
        lines.append('# CFFI objects')
        lines.append('ffi: FFI')
        lines.append('lib: Any')

        return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description='Generate .pyi stubs from C declarations')
    parser.add_argument('input', help='Input C file containing declarations')
    parser.add_argument('output', help='Output .pyi file')
    parser.add_argument('--inline', help='Parse inline C code instead of file')

    args = parser.parse_args()

    converter = CFFIToPyiConverter()

    if args.inline:
        c_code = args.inline
    else:
        input_file = Path(args.input)
        if not input_file.exists():
            print(f"Input file {input_file} not found", file=sys.stderr)
            return 1

        with open(input_file, 'r', encoding='utf-8') as f:
            c_code = f.read()

    # Parse C code
    converter.parse_c_code(c_code)

    # Generate .pyi content
    pyi_content = converter.generate_pyi()

    # Write output
    output_file = Path(args.output)
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(pyi_content)

    print(f"Generated {output_file}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
