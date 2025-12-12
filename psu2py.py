#!/usr/bin/env python3
"""
Translate decision table CSV pseudocode (.psu) to Python module.

Usage: python3 psu2py.py table.psu > table.py

Generated with Claude Code (https://claude.ai/code)
Co-Authored-By: Claude <noreply@anthropic.com>
"""

import sys
import os
import csv
import io
import re


def to_py_ident(s):
    """Convert a string to a valid Python identifier."""
    result = re.sub(r'[^A-Za-z0-9]', '_', s)
    if result and result[0].isdigit():
        result = '_' + result
    return result


def parse_csv_line(line):
    """Parse a CSV line into fields."""
    reader = csv.reader(io.StringIO(line))
    for row in reader:
        return row
    return []


def parse_psu(lines):
    """Parse CSV pseudocode into metadata and body."""
    inputs = {}      # var -> [values]
    outputs = {}     # var -> [values]
    depth = None
    body = []

    for line in lines:
        line = line.rstrip()
        if not line:
            continue

        fields = parse_csv_line(line)
        if not fields:
            continue

        cmd = fields[0]

        if cmd == 'I':
            # Input variable: I,var,val
            var, val = fields[1], fields[2]
            if var not in inputs:
                inputs[var] = []
            if val not in inputs[var]:
                inputs[var].append(val)
        elif cmd == 'O':
            # Output variable: O,var,val
            var, val = fields[1], fields[2]
            if var not in outputs:
                outputs[var] = []
            if val not in outputs[var]:
                outputs[var].append(val)
        elif cmd == 'D':
            # Depth: D,n
            depth = int(fields[1])
        elif cmd in ('L', 'J', 'T', 'R'):
            body.append((cmd, fields[1:]))

    return inputs, outputs, depth, body


def generate_python(name, inputs, outputs, depth, body):
    """Generate Python code from parsed pseudocode."""
    lines = []

    # Header
    lines.append(f'"""Generated from {name}.dtc - do not edit"""')
    lines.append('')
    lines.append('from enum import IntEnum, auto')
    lines.append('')

    # Input enums
    for var, vals in inputs.items():
        py_var = to_py_ident(var)
        lines.append(f'class {py_var}(IntEnum):')
        for val in sorted(vals):
            py_val = to_py_ident(val)
            lines.append(f'  {py_val} = auto()')
        lines.append('')

    # Output enums
    for var, vals in outputs.items():
        py_var = to_py_ident(var)
        lines.append(f'class {py_var}(IntEnum):')
        for val in sorted(vals):
            py_val = to_py_ident(val)
            lines.append(f'  {py_val} = auto()')
        lines.append('')

    # Function signature
    input_params = ', '.join(to_py_ident(var) for var in inputs.keys())
    lines.append(f'def evaluate({input_params}):')
    if depth:
        lines.append(f'  """Evaluate decision table (max depth: {depth})"""')

    # Initialize output variables
    for var in outputs.keys():
        py_var = to_py_ident(var)
        lines.append(f'  _{py_var} = None')

    # State machine
    lines.append('  _s = 0')
    lines.append('  while True:')

    # Process body - need to look ahead for fallthrough
    current_state = 0
    need_state = True
    indent = '      '

    for i, (cmd, args) in enumerate(body):
        # Look ahead to find next label for fallthrough
        next_label = None
        for j in range(i + 1, len(body)):
            if body[j][0] == 'L':
                lbl = body[j][1][0]
                if lbl != '0':
                    next_label = lbl
                break

        if cmd == 'L':
            # Label: L,n
            label = args[0]
            if label != '0':
                current_state = int(label)
                need_state = True
        elif cmd == 'T':
            # Test: T,var,val,target
            var, val, target = args[0], args[1], args[2]
            py_var = to_py_ident(var)
            py_val = to_py_ident(val)
            if need_state:
                lines.append(f'    if _s == {current_state}:')
                need_state = False
            lines.append(f'{indent}if {py_var} == {py_var}.{py_val}:')
            lines.append(f'{indent}  _s = {target}')
            lines.append(f'{indent}  continue')
            # Add fallthrough to next label
            if next_label:
                lines.append(f'{indent}_s = {next_label}')
                lines.append(f'{indent}continue')
        elif cmd == 'J':
            # Jump: J,target
            target = args[0]
            if need_state:
                lines.append(f'    if _s == {current_state}:')
                need_state = False
            if target == '0':
                # Return
                output_tuple = ', '.join(f'_{to_py_ident(var)}' for var in outputs.keys())
                lines.append(f'{indent}return ({output_tuple})')
            else:
                lines.append(f'{indent}_s = {target}')
                lines.append(f'{indent}continue')
        elif cmd == 'R':
            # Resolve: R,var,val
            var, val = args[0], args[1]
            py_var = to_py_ident(var)
            py_val = to_py_ident(val)
            if need_state:
                lines.append(f'    if _s == {current_state}:')
                need_state = False
            lines.append(f'{indent}_{py_var} = {py_var}.{py_val}')

    lines.append('')
    return '\n'.join(lines)


def main():
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} file.psu', file=sys.stderr)
        sys.exit(1)

    filename = sys.argv[1]
    name = os.path.basename(filename)
    if name.endswith('.psu'):
        name = name[:-4]

    with open(filename) as f:
        lines = f.readlines()

    inputs, outputs, depth, body = parse_psu(lines)
    code = generate_python(name, inputs, outputs, depth, body)
    print(code)


if __name__ == '__main__':
    main()
