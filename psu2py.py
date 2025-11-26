#!/usr/bin/env python3
"""
Translate decision table pseudocode (.psu) to Python module.

Usage: python3 psu2py.py table.psu > table.py

Generated with Claude Code (https://claude.ai/code)
Co-Authored-By: Claude <noreply@anthropic.com>
"""

import sys
import os


def parse_psu(lines):
    """Parse pseudocode into metadata and body."""
    inputs = {}      # var -> [values]
    outputs = {}     # var -> [values]
    depth = None
    body = []

    for line in lines:
        line = line.rstrip()
        if line.startswith('# E '):
            # Input variable: # E var val
            parts = line.split()
            var, val = parts[2], parts[3]
            if var not in inputs:
                inputs[var] = []
            if val not in inputs[var]:
                inputs[var].append(val)
        elif line.startswith('# R '):
            # Output variable: # R var val
            parts = line.split()
            var, val = parts[2], parts[3]
            if var not in outputs:
                outputs[var] = []
            if val not in outputs[var]:
                outputs[var].append(val)
        elif line.startswith('# D '):
            # Depth: # D n
            depth = int(line.split()[2])
        elif line and line[0] in ':<>=':
            body.append(line)

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
        lines.append(f'class {var}(IntEnum):')
        for val in sorted(vals):
            lines.append(f'  {val} = auto()')
        lines.append('')

    # Output enums
    for var, vals in outputs.items():
        lines.append(f'class {var}(IntEnum):')
        for val in sorted(vals):
            lines.append(f'  {val} = auto()')
        lines.append('')

    # Function signature
    input_params = ', '.join(inputs.keys())
    lines.append(f'def evaluate({input_params}):')
    if depth:
        lines.append(f'  """Evaluate decision table (max depth: {depth})"""')

    # Initialize output variables
    for var in outputs.keys():
        lines.append(f'  _{var} = None')

    # State machine
    lines.append('  _s = 0')
    lines.append('  while True:')

    # Process body - need to look ahead for fallthrough
    current_state = 0
    need_state = True
    indent = '      '

    for i, stmt in enumerate(body):
        # Look ahead to find next label for fallthrough
        next_label = None
        for j in range(i + 1, len(body)):
            if body[j].startswith('< '):
                lbl = body[j][2:]
                if lbl != '0':
                    next_label = lbl
                break

        if stmt.startswith('< '):
            # Label: < n
            label = stmt[2:]
            if label != '0':
                current_state = int(label)
                need_state = True
        elif stmt.startswith(': '):
            # Conditional: : var val > n
            rest = stmt[2:]
            parts = rest.split(' > ')
            var_val = parts[0].split()
            var, val = var_val[0], var_val[1]
            target = parts[1]
            if need_state:
                lines.append(f'    if _s == {current_state}:')
                need_state = False
            lines.append(f'{indent}if {var} == {var}.{val}:')
            lines.append(f'{indent}  _s = {target}')
            lines.append(f'{indent}  continue')
            # Add fallthrough to next label
            if next_label:
                lines.append(f'{indent}_s = {next_label}')
                lines.append(f'{indent}continue')
        elif stmt.startswith('> '):
            # Jump: > n
            target = stmt[2:]
            if need_state:
                lines.append(f'    if _s == {current_state}:')
                need_state = False
            if target == '0':
                # Return
                output_tuple = ', '.join(f'_{var}' for var in outputs.keys())
                lines.append(f'{indent}return ({output_tuple})')
            else:
                lines.append(f'{indent}_s = {target}')
                lines.append(f'{indent}continue')
        elif stmt.startswith('= '):
            # Assignment: = var val
            rest = stmt[2:]
            idx = rest.index(' ')
            var = rest[:idx]
            val = rest[idx + 1:]
            if need_state:
                lines.append(f'    if _s == {current_state}:')
                need_state = False
            lines.append(f'{indent}_{var} = {var}.{val}')

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
