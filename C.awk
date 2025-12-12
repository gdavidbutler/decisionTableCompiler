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

# Generates C header and implementation from decision table CSV pseudocode
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

# Parse CSV fields from a line into fields[] array, return count
# Handles quoted fields with embedded commas and doubled quotes
function parse_csv(line, fields,    n, i, c, in_quote, field) {
  n = 0
  i = 1
  while (i <= length(line)) {
    field = ""
    in_quote = 0
    c = substr(line, i, 1)
    # Check if field starts with quote
    if (c == "\"") {
      in_quote = 1
      i++
      while (i <= length(line)) {
        c = substr(line, i, 1)
        if (c == "\"") {
          # Check for doubled quote
          if (substr(line, i + 1, 1) == "\"") {
            field = field "\""
            i += 2
          } else {
            # End of quoted field
            i++
            break
          }
        } else {
          field = field c
          i++
        }
      }
      # Skip comma after quoted field
      if (substr(line, i, 1) == ",")
        i++
    } else {
      # Unquoted field - read until comma or end
      while (i <= length(line)) {
        c = substr(line, i, 1)
        if (c == ",") {
          i++
          break
        }
        field = field c
        i++
      }
    }
    fields[++n] = field
  }
  return n
}

# Convert a string to valid C identifier
# Replace non-alphanumeric with underscore, ensure doesn't start with digit
function to_c_ident(s,    result, i, c) {
  result = ""
  for (i = 1; i <= length(s); i++) {
    c = substr(s, i, 1)
    if (c ~ /[A-Za-z0-9]/)
      result = result c
    else
      result = result "_"
  }
  # Ensure doesn't start with digit
  if (result ~ /^[0-9]/)
    result = "_" result
  return result
}

# Parse input variable metadata: I,var,val
/^I,/ {
  parse_csv($0, f)
  var = f[2]
  val = f[3]
  if (!seen_input[var]++) {
    input_vars[++num_inputs] = var
  }
  input_vals[var, ++input_val_count[var]] = val
  all_vals[val]++
  next
}

# Parse output variable metadata: O,var,val
/^O,/ {
  parse_csv($0, f)
  var = f[2]
  val = f[3]
  if (!seen_output[var]++) {
    output_vars[++num_outputs] = var
  }
  output_vals[var, ++output_val_count[var]] = val
  all_vals[val]++
  next
}

# Parse depth: D,n
/^D,/ {
  parse_csv($0, f)
  depth = f[2]
  next
}

# Store body lines for two-pass processing
/^[LJTR],/ {
  body[++num_body] = $0
}

END {
  if (num_body > 0) {
    # First pass: collect jump targets
    for (i = 1; i <= num_body; i++) {
      line = body[i]
      if (line ~ /^T,/) {
        # Test: T,var,val,label
        parse_csv(line, f)
        label = f[4]
        jump_target[label] = 1
      } else if (line ~ /^J,/) {
        # Jump: J,label
        parse_csv(line, f)
        label = f[2]
        jump_target[label] = 1
      }
    }

    emit_header()

    # Second pass: emit code, filtering unused labels
    for (i = 1; i <= num_body; i++) {
      line = body[i]
      if (line ~ /^L,/) {
        # Label: L,n
        parse_csv(line, f)
        label = f[2]
        if (label != "0" && jump_target[label])
          print "L" label ":" > cfile
      } else if (line ~ /^T,/) {
        # Test: T,var,val,label
        parse_csv(line, f)
        var = f[2]
        val = f[3]
        label = f[4]
        c_var = to_c_ident(var)
        c_val = to_c_ident(val)
        print "  if (" c_var " == " name "_" c_var "_" c_val ")" > cfile
        print "    goto L" label ";" > cfile
      } else if (line ~ /^J,/) {
        # Jump: J,label
        parse_csv(line, f)
        label = f[2]
        print "  goto L" label ";" > cfile
      } else if (line ~ /^R,/) {
        # Resolve: R,var,val
        parse_csv(line, f)
        var = f[2]
        val = f[3]
        c_var = to_c_ident(var)
        c_val = to_c_ident(val)
        print "  *" c_var " = " name "_" c_var "_" c_val ";" > cfile
      }
    }

    emit_footer()
  }
}

function emit_header(    i, j, k, var, val, vals, n, c_var, c_val) {
  # === HEADER FILE ===
  # Header guard start
  print "#ifndef " name_upper "_H" > hfile
  print "#define " name_upper "_H" > hfile
  print "" > hfile

  # Emit enum for each input variable
  for (i = 1; i <= num_inputs; i++) {
    var = input_vars[i]
    c_var = to_c_ident(var)
    print "enum " name "_" c_var "_e {" > hfile

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
      c_val = to_c_ident(vals[j])
      if (j == 1)
        print " " name "_" c_var "_" c_val > hfile
      else
        print "," name "_" c_var "_" c_val > hfile
    }
    print "};" > hfile
    print "" > hfile
  }

  # Emit enum for each output variable
  for (i = 1; i <= num_outputs; i++) {
    var = output_vars[i]
    c_var = to_c_ident(var)
    print "enum " name "_" c_var "_e {" > hfile

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
      c_val = to_c_ident(vals[j])
      if (j == 1)
        print " " name "_" c_var "_" c_val > hfile
      else
        print "," name "_" c_var "_" c_val > hfile
    }
    print "};" > hfile
    print "" > hfile
  }

  # Emit function declaration
  print "void" > hfile
  print func_name "(" > hfile

  for (i = 1; i <= num_inputs; i++) {
    var = input_vars[i]
    c_var = to_c_ident(var)
    if (i == 1)
      print "  enum " name "_" c_var "_e " c_var > hfile
    else
      print " ,enum " name "_" c_var "_e " c_var > hfile
  }

  for (i = 1; i <= num_outputs; i++) {
    var = output_vars[i]
    c_var = to_c_ident(var)
    print " ,enum " name "_" c_var "_e *" c_var > hfile
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
    c_var = to_c_ident(var)
    if (i == 1)
      print "  enum " name "_" c_var "_e " c_var > cfile
    else
      print " ,enum " name "_" c_var "_e " c_var > cfile
  }

  for (i = 1; i <= num_outputs; i++) {
    var = output_vars[i]
    c_var = to_c_ident(var)
    print " ,enum " name "_" c_var "_e *" c_var > cfile
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
