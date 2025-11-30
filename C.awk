#!awk -f
#
# decisionTableCompiler - generate optimal pseudocode for decision tables
# Copyright (C) 1993-2025 G. David Butler <gdb@dbSystems.com>
#
# This file is part of decisionTableCompiler
#
# decisionTableCompiler is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# decisionTableCompiler is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

# Generates C header and implementation from decision table pseudocode with metadata
# Usage: awk -f C.awk file.psu
# Generates file.h and file.c

BEGIN {
  # Extract name from filename or use "stdin"
  if (ARGC > 1 && ARGV[1] != "-") {
    fname = ARGV[1]
    # Remove path
    sub(/^.*\//, "", fname)
    # Remove .psu extension
    sub(/\.psu$/, "", fname)
    name = fname
  } else {
    name = "stdin"
  }

  # Set output filenames
  hfile = name ".h"
  cfile = name ".c"

  # Convert name to uppercase for header guard
  name_upper = toupper(name)

  # Function name is nameCamelCase
  func_name = name "Evaluate"
}

# Parse input variable metadata: # E var val
/^# E / {
  var = $3
  val = $4
  if (!seen_input[var]++) {
    input_vars[++num_inputs] = var
  }
  input_vals[var, ++input_val_count[var]] = val
  all_vals[val]++
  next
}

# Parse output variable metadata: # R var val
/^# R / {
  var = $3
  val = $4
  if (!seen_output[var]++) {
    output_vars[++num_outputs] = var
  }
  output_vals[var, ++output_val_count[var]] = val
  all_vals[val]++
  next
}

# Parse depth comment: # D n
/^# D / {
  depth = $3
  next
}

# Store body lines for two-pass processing
/^[:<>=] / {
  body[++num_body] = $0
}

END {
  if (num_body > 0) {
    # First pass: collect jump targets
    for (i = 1; i <= num_body; i++) {
      line = body[i]
      if (line ~ /^: /) {
        # Conditional: : var val > label
        idx = index(line, " > ")
        label = substr(line, idx + 3)
        jump_target[label] = 1
      } else if (line ~ /^> /) {
        # Unconditional jump
        label = substr(line, 3)
        jump_target[label] = 1
      }
    }

    emit_header()

    # Second pass: emit code, filtering unused labels
    for (i = 1; i <= num_body; i++) {
      line = body[i]
      if (line ~ /^< /) {
        # Label definition - only emit if it is a jump target
        label = substr(line, 3)
        if (label != "0" && jump_target[label])
          print "L" label ":" > cfile
      } else if (line ~ /^: /) {
        # Conditional: : var val > label
        rest = substr(line, 3)
        split(rest, parts, " ")
        var = parts[1]
        val = parts[2]
        idx = index(line, " > ")
        label = substr(line, idx + 3)
        print "  if (" var " == " name "_" var "_" val ")" > cfile
        print "    goto L" label ";" > cfile
      } else if (line ~ /^> /) {
        # Unconditional jump
        label = substr(line, 3)
        print "  goto L" label ";" > cfile
      } else if (line ~ /^= /) {
        # Assignment: = var val
        rest = substr(line, 3)
        idx = index(rest, " ")
        var = substr(rest, 1, idx - 1)
        val = substr(rest, idx + 1)
        print "  *" var " = " name "_" var "_" val ";" > cfile
      }
    }

    emit_footer()
  }
}

function emit_header(    i, j, k, var, val, vals, n) {
  # === HEADER FILE ===
  # Header guard start
  print "#ifndef " name_upper "_H" > hfile
  print "#define " name_upper "_H" > hfile
  print "" > hfile

  # Emit enum for each input variable
  for (i = 1; i <= num_inputs; i++) {
    var = input_vars[i]
    print "enum " name "_" var "_e {" > hfile

    # Sort values for this variable
    n = input_val_count[var]
    for (j = 1; j <= n; j++) {
      vals[j] = input_vals[var, j]
    }
    for (j = 1; j < n; j++) {
      for (k = j + 1; k <= n; k++) {
        if (vals[j] > vals[k]) {
          val = vals[j]
          vals[j] = vals[k]
          vals[k] = val
        }
      }
    }

    # Emit enum values with name and variable prefix
    for (j = 1; j <= n; j++) {
      if (j == 1)
        print " " name "_" var "_" vals[j] > hfile
      else
        print "," name "_" var "_" vals[j] > hfile
    }
    print "};" > hfile
    print "" > hfile
  }

  # Emit enum for each output variable
  for (i = 1; i <= num_outputs; i++) {
    var = output_vars[i]
    print "enum " name "_" var "_e {" > hfile

    # Sort values for this variable
    n = output_val_count[var]
    for (j = 1; j <= n; j++) {
      vals[j] = output_vals[var, j]
    }
    for (j = 1; j < n; j++) {
      for (k = j + 1; k <= n; k++) {
        if (vals[j] > vals[k]) {
          val = vals[j]
          vals[j] = vals[k]
          vals[k] = val
        }
      }
    }

    # Emit enum values with name and variable prefix
    for (j = 1; j <= n; j++) {
      if (j == 1)
        print " " name "_" var "_" vals[j] > hfile
      else
        print "," name "_" var "_" vals[j] > hfile
    }
    print "};" > hfile
    print "" > hfile
  }

  # Emit function declaration
  print "void" > hfile
  print func_name "(" > hfile

  for (i = 1; i <= num_inputs; i++) {
    var = input_vars[i]
    if (i == 1)
      print "  enum " name "_" var "_e " var > hfile
    else
      print " ,enum " name "_" var "_e " var > hfile
  }

  for (i = 1; i <= num_outputs; i++) {
    var = output_vars[i]
    print " ,enum " name "_" var "_e *" var > hfile
  }

  print ");" > hfile

  # === C FILE ===
  # Include header
  print "#include \"" name ".h\"" > cfile
  print "" > cfile

  # Function definition
  print "void" > cfile
  print func_name "(" > cfile

  for (i = 1; i <= num_inputs; i++) {
    var = input_vars[i]
    if (i == 1)
      print "  enum " name "_" var "_e " var > cfile
    else
      print " ,enum " name "_" var "_e " var > cfile
  }

  for (i = 1; i <= num_outputs; i++) {
    var = output_vars[i]
    print " ,enum " name "_" var "_e *" var > cfile
  }

  print "){" > cfile
  if (depth != "") {
    print "  /* " depth " */" > cfile
  }
}

function emit_footer() {
  # Close header guard
  print "" > hfile
  print "#endif" > hfile

  # Close function in C file with single exit point
  print "L0:" > cfile
  print "  return;" > cfile
  print "}" > cfile
}
