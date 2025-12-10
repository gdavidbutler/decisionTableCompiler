# Decision Table Compiler (dtc)

Turn spreadsheet decision tables into optimized code.

## The Idea

Maintain business logic in a spreadsheet. Compiler generates efficient code.

**Spreadsheet RFC 4180 (CSV) format:**
```
# @name (@ prefix) starts a new decision (sub)table for name depending on name(s)

@proceed,signal,canStop
yes,green,
yes,yellow,no
no,yellow,yes
no,red,
```

**Pseudocode Output Translated (by Hand) to Nested if/else:**
```c
if (signal == yellow) {
  if (canStop == no)
    proceed = yes;
  else
    proceed = no;
} else {
  if (signal == green)
    proceed = yes;
  else
    proceed = no;
}
```

When requirements change, update the spreadsheet and regenerate. No manual refactoring of nested if/else chains. Example translators for C and Python are included.

## Quick Start

```bash
# Build (only requires an ANSI C compiler)
make

# Compile decision table to pseudocode
./dtc table.dtc > table.psu

# Generate C code
awk -f C.awk table.psu
# Creates: table.h and table.c

# Generate Python code
python3 psu2py.py table.psu > table.py

Try the included examples:
```bash
make examples
./test
make examples-py
python3 test.py
```

## Why Use This?

**The problem:** Decision logic in code is fragile. A small requirements change can require reorganizing deeply nested if/else statements. Hand-optimization is error-prone and hard to review.

**The solution:** Express requirements as a decision table (each row is one rule). The compiler automatically:
- Finds the optimal evaluation order
- Eliminates duplicate code paths
- Minimizes decision depth
- Generates consistent, reviewable output

Domain experts maintain the spreadsheet. Programmers integrate the generated code. Requirements stay traceable.

## Real-World Origin

An early version of this program was developed in 1993 for avionics and embedded systems where:
- Requirements change frequently but must remain traceable
- Domain experts (not programmers) maintain the logic
- Memory and execution time are constrained
- Safety standards require deterministic behavior

The included `power.dtc` is an early version of an actual twin-engine aircraft power determination decision table.

## How It Works

```
Decision Table (.dtc)  →  Pseudocode (.psu)  →  Target Language (.h, .c, etc.)
     CSV Format              Optimized DAG         Language-specific code
```

1. **Input**: CSV decision tables where each row defines one outcome
2. **Compilation**: `dtc` builds an optimized decision graph (DAG structure)
3. **Output**: Pseudocode with metadata documenting all variables
4. **Translation**: Language-specific tools (e.g., `C.awk`) generate final code

## Input Format

Disjunctive normal form CSV format using a prefix character, '@', to indicate a decision (sub)table:

```csv
@resultName,dependentName1,dependentName2,...
resultValue1,dependentName1Value1,dependentName2Value1,...
resultValue2,dependentName1Value2,dependentName2Value2,...
...
```

### Critical Constraints

**Input variable values must be mutually exclusive and exhaustive:**
- **Mutually exclusive**: Each value represents a distinct state (e.g., a signal cannot be both "green" and "red")
- **Exhaustive**: The values must cover all possible states of the variable (e.g., "green", "red", "yellow" covers all traffic light states)
- **Empty cells**: Indicate "any value" - the result does not depend on that input variable for this row

These constraints ensure:
1. The decision table is **deterministic** - every possible input combination maps to exactly one output
2. The decision table is **complete** - no input combination is undefined
3. The compiler can **optimize safely** - knowing all possible values enables better test ordering

Example (traffic light decision as input to two other tables):
```csv
# Proceed when the light is green and don't when the light is red.
@proceed,signal
yes,green
no,red

# When the light is yellow, don't proceed when a stop can be done safely, otherwise proceed.
@proceed,signal,canStop
yes,yellow,no
no,yellow,yes

# Apply brake if not proceeding, otherwise don't.
@brake,proceed
yes,no
no,yes

# Apply accelerator when proceed is decided and the light is judged to be close to changing, otherwise don't.
@accelerator,proceed,isClose
yes,yes,yes
no,yes,no
no,no,
```

Empty cells represent "any value". The compiler finds the optimal way to evaluate these conditions.

## Output Format

Optimized single decision tree for all tables in pseudo-code with metadata:
```
# E canStop no          // Input variable metadata
# E canStop yes
# E isClose no
# E isClose yes
# E signal green
# E signal red
# E signal yellow
# R accelerator no      // Output variable metadata
# R accelerator yes
# R brake no
# R brake yes
# R proceed no
# R proceed yes
# D 3                   // Maximum number of evaluations required
: signal yellow > 1     // Test signal, goto 1 if yellow
< 2                     // Label 2 (signal not yellow)
: signal green > 3      // Test signal, goto 3 if green
< 4                     // Label 4 (signal is red)
= accelerator no        // Set accelerator to no
= brake yes             // Set brake to yes
= proceed no            // Set proceed to no
> 0                     // Exit
< 3                     // Label 3 (proceed: green or yellow+can't stop)
= brake no              // Set brake to no
= proceed yes           // Set proceed to yes
: isClose no > 5        // Test isClose, goto 5 if no
< 6                     // Label 6 (isClose is yes)
= accelerator yes       // Set accelerator to yes
> 0                     // Exit
< 5                     // Label 5 (isClose is no)
= accelerator no        // Set accelerator to no
> 0                     // Exit
< 1                     // Label 1 (signal is yellow)
: canStop no > 3        // Test canStop, goto 3 (proceed) if no
> 4                     // Otherwise goto 4 (stop)
< 0                     // Exit label
```

### Pseudocode Syntax

**Metadata Lines:**
- **`# E var val`** - Expression (input) variable with possible value - provides type information
- **`# R var val`** - Result (output) variable with possible value - provides type information
- **`# D n`** - Maximum decision depth (worst-case tests to reach a leaf) - complexity metric

**Code Lines:**
- **`< n`** - Label definition (numeric, 0 is exit)
- **`: var val > n`** - Conditional test and jump
- **`> n`** - Unconditional jump (0 = exit/return)
- **`= var val`** - Assignment

**Metadata Purpose:**
The metadata lines enable automatic generation of:
- Type-safe code with enums in statically-typed languages
- Function signatures with correct parameter types
- Visual diagrams (UML, flowcharts) without parsing code
- Documentation showing all possible values for each variable

This metadata makes the pseudocode self-describing and sufficient for translation to any programming language or visualization format.

## Language Translation

The pseudocode is designed to map efficiently to machine instructions, using goto for optimal performance. The metadata (# E and # R lines) provides all information needed to generate type definitions and function signatures in any target language.

### Direct Translation (goto-based)

Languages with goto support can translate the pseudocode almost directly, preserving the optimized DAG structure:

| Language | goto Support | Translator | Notes |
|----------|--------------|------------|-------|
| **C** | Full | `C.awk` | Direct translation, optimal performance |
| **C++** | Full | `C.awk` | Same as C |
| **Go** | Full | Similar to `C.awk` | Minor syntax differences (braces vs semicolons) |

**Example (C):**
```bash
awk -f C.awk table.psu
# Creates: table.h and table.c
```

This generates complete C code with:
- Separate enum for each variable with namespaced values
- Header file with type definitions and function declaration
- Implementation file with goto-based decision logic preserving DAG structure
- All identifiers prefixed with table name to avoid conflicts

### State Machine Translation

Languages without goto require translation to a state machine pattern. This adds dispatch overhead but maintains the DAG structure and compact representation:

| Language | Recommended Pattern | Translator |
|----------|---------------------|------------|
| **Python** | `while` + `if` chain | `psu2py.py` (included) |
| **Rust** | `loop` + `match` with State enum | (not included) |
| **Java** | `while` + `switch` on numeric state | (not included) |
| **JavaScript** | `while` + `switch` on numeric state | (not included) |

**How It Works:**
State machine translators convert labels to state IDs and use a loop with if/switch statement for dispatch. The DAG structure is preserved - nodes still converge, preventing code duplication.

**Tradeoffs:**
- Preserves DAG structure (no code duplication)
- Maintains compact representation
- Modern compilers optimize switch statements to jump tables
- Adds state variable and dispatch overhead compared to goto
- May lose some compiler optimizations available with goto

**Performance:**
While not as optimal as goto, state machine translation still benefits from the dtc's optimization work (minimized depth, maximized sharing). The overhead is primarily the state variable update and switch dispatch, not the decision logic itself.

### Why goto-based Pseudocode?

The decision table compiler generates a **directed acyclic graph (DAG)** where multiple decision paths converge at shared nodes. This structure:

1. **Eliminates duplicate code** - Tree-structured if/else would duplicate downstream logic at every branch point (exponential bloat)
2. **Maps to machine code** - goto translates directly to CPU jump instructions
3. **Avoids nesting limits** - Flat structure prevents deeply nested if/else hitting compiler limits
4. **Enables sharing** - Multiple paths to same outcome share the leaf node code

Example: In `power.psu`, multiple decision paths converge to the same power state outputs (BATT, ONE_GEN_FLGT, etc.), sharing result assignment code.

## C Code Generation

The `C.awk` script translates pseudocode into complete C header and implementation files with proper type safety and namespace isolation.

### Quick Start

```bash
# Generate .psu pseudocode from decision table
./dtc table.dtc > table.psu

# Generate C header and implementation (single invocation)
awk -f C.awk table.psu
# Creates: table.h and table.c
```

Or use the Makefile:
```bash
# Build examples (automatically generates .psu, .h, and .c files)
make examples

# This generates from .dtc → .psu → .h/.c:
# - power.dtc → power.psu → power.h, power.c
# - DisjunctiveNormalForm.dtc → DisjunctiveNormalForm.psu → DisjunctiveNormalForm.h, DisjunctiveNormalForm.c
# - test (executable demonstrating both tables)
```

The Makefile includes pattern rules to automatically generate `.psu` from `.dtc` files and `.h`/`.c` from `.psu` files.

## Python Code Generation

The `psu2py.py` script translates pseudocode into Python modules using a state machine pattern.

### Quick Start

```bash
# Generate .psu pseudocode from decision table
./dtc table.dtc > table.psu

# Generate Python module
python3 psu2py.py table.psu > table.py
```

Or use the Makefile:
```bash
make examples-py
```

### Generated Code Example

**Input (decision.psu):**
```
# E signal green
# E signal red
# R proceed no
# R proceed yes
: signal green > 1
< 2
= proceed no
> 0
< 1
= proceed yes
> 0
< 0
```

**Output (decision.py):**
```python
from enum import IntEnum, auto

class signal(IntEnum):
  green = auto()
  red = auto()

class proceed(IntEnum):
  no = auto()
  yes = auto()

def evaluate(signal):
  _proceed = None
  _s = 0
  while True:
    if _s == 0:
      if signal == signal.green:
        _s = 1
        continue
      _s = 2
      continue
    if _s == 2:
      _proceed = proceed.no
      return (_proceed)
    if _s == 1:
      _proceed = proceed.yes
      return (_proceed)
```

**Usage:**
```python
import decision

result = decision.evaluate(decision.signal.green)
print(result.name)  # "yes"
```

## Visualization and Documentation

The pseudocode metadata provides complete information for generating visual representations of decision logic.

### Flowcharts and UML Diagrams

The metadata (# E, # R, # D) combined with the pseudocode structure contains all necessary information to generate:

**UML Activity Diagrams:**
- Decision nodes (diamonds) from `: var val > n` conditionals
- Action nodes (rectangles) from `= var val` assignments
- Control flow from `> n` jumps
- Entry point from start of code (before first label)
- Exit point from label 0 or `> 0` jumps
- Guard conditions from test expressions

**Flowcharts via Graphviz/DOT:**
The pseudocode directly translates to DOT format for visualization:

```bash
# Hypothetical converter (not included)
awk -f psu2dot.awk power.psu > power.dot
dot -Tsvg power.dot > power.svg
```

The generated graph would show:
- Decision nodes (circles) for intermediate states
- Result nodes (boxes) with assignments
- Edges labeled with conditions
- DAG structure showing path convergence

**Metadata Completeness:**
The pseudocode contains everything needed for diagram generation:
- All variable names and their possible values (# E, # R)
- Complete control flow graph (labels and jumps)
- Decision conditions with explicit comparisons
- Complexity metric (# D) for documentation
- Entry and exit points clearly marked

This allows domain experts to review decision logic visually while maintaining a single source of truth (the .dtc file).

## Computational Complexity

The decision table optimization problem is **NP-complete**. Finding the optimal decision tree that minimizes worst-case evaluation depth while maximizing node sharing involves exploring an exponentially large search space.

### Compilation Time

**Compilation time varies based on:**
- **Number of input variables**: Each additional variable exponentially increases the search space
- **Number of unique values per variable**: More values = more possible test orderings

**Practical expectations:**
- **Simple tables** (3-5 variables): Milliseconds
- **Medium tables** (8-10 variables): Seconds
- **Complex tables** (15+ variables): Minutes or longer

### Why Optimization Matters

**Without optimization**, a naive implementation might:
- Test every condition sequentially (maximum depth)
- Duplicate code at every branch point (exponential size)

**With dtc optimization**, the `power.dtc` example (10 input variables, 20 values):
- Achieves depth 8 instead of depth 20 (60% reduction). (With the -q, quick, flag, the depth is 10.)
- Shares nodes via DAG structure (compact output)

Compilation time is paid once. The generated code runs in O(depth) time.

### If Compilation Is Slow

- **This is expected** for complex tables - the tool is doing hard optimization work
- Use the -q (quick) flag to stop on the first complete heuristic pass. The result is correct but, probably, not optimal.
- Either way, the output will be much better than hand-written nested if/else

### License

- GNU Public License (GPL) program
- Like any compiler, the input decision tables and the output pseudocode belong to you
