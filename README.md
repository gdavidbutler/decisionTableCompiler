# Decision Table Compiler (dtc)

Turn spreadsheet (in [CSV](https://en.wikipedia.org/wiki/Comma-separated_values) format) [decision table](https://en.wikipedia.org/wiki/Decision_table)s into optimized code.

## The Idea

Maintain business logic in a spreadsheet. Compiler generates efficient code.

**Spreadsheet CSV [RFC 4180](https://tools.ietf.org/rfc/rfc4180.txt) format:**
```
# @name (@ prefix) starts a new decision (sub)table for name depending on name(s)

@proceed,signal,canStop
yes,green,
yes,yellow,no
no,yellow,yes
no,red,
```

**Pseudocode output translated (by hand) to C language style nested if/else:**
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

**The solution:** Express requirements as a decision table. The compiler automatically:
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

1. **Input**: CSV formatted decision tables
2. **Compilation**: `dtc` builds an optimized decision [directed acyclic graph](https://en.wikipedia.org/wiki/Directed_acyclic_graph) (DAG)
3. **Output**: CSV formatted pseudocode with metadata documenting all names' values, indicating which are independent (input) and dependent (output)
4. **Translation**: Language-specific tools (e.g., `C.awk`) generate final code

## Input Format

[Disjunctive normal form](https://en.wikipedia.org/wiki/Disjunctive_normal_form) CSV format using a prefix character, '@', to indicate a decision (sub)table:

```csv
@resultName,dependentName1,dependentName2,...
resultValue1,dependentName1Value1,dependentName2Value1,...
resultValue2,dependentName1Value2,dependentName2Value2,...
...
```

### Critical Constraints

**Names' values must be mutually exclusive and exhaustive:**
- **Mutually exclusive**: Each value represents a distinct state (e.g., a signal cannot be both "green" and "red")
- **Exhaustive**: The values must cover all possible states of the name (e.g., "green", "red", "yellow" covers all traffic light states)

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

### Names with Spaces and Special Characters

Variable names and values can contain spaces, punctuation, and other special characters. The compiler uses standard CSV encoding (RFC 4180) for both input and output:

```csv
@Free shipping,Favored customer,Total order amount > $500.00
y,y,
y,n,y
n,n,n
```

Names containing commas or quotes require CSV quoting:
```csv
@result,"Order requires special ""HAZMAT""",destination
y,y,domestic
n,y,international
```

The translators (`C.awk`, `psu2py.py`) convert these to valid identifiers in the target language by replacing special characters with underscores.

### Sparse vs Fully Specified Tables

Decision tables can be written in two styles:

**Fully Specified Tables:**
Every possible combination of input values has an explicit row. For n binary inputs, this means 2^n rows.

```csv
@result,A,B
yes,0,0
yes,0,1
no,1,0
no,1,1
```

**Sparse Tables:**
Empty cells indicate "any value" - the result does not depend on that input for this row. This reduces table size and often improves readability:

```csv
@result,A,B
yes,0,
no,1,
```

Both tables above are equivalent. The sparse form has 2 rows instead of 4.

**Optimization implications:**
- Sparse tables give the compiler more freedom to reorder tests, often producing better results
- Fully specified tables may constrain the search space, sometimes compiling faster
- Domain experts often find sparse tables easier to maintain since each row captures a logical rule rather than enumerating combinations

## Table Design Patterns

### Conditions-Rules-Actions Pattern

Many decision table methodologies use a two-stage approach:

1. **Stage 1 (Conditions → Rules):** Input conditions are evaluated to determine which "rule" applies
2. **Stage 2 (Rules → Actions):** The matched rule determines which actions fire

This pattern maps naturally to dtc using an intermediate name e.g. "rule":

```csv
# Stage 1: Conditions determine which rule matches
@rule,customer type,order value,destination
1,premium,,
2,standard,high,domestic
3,standard,high,international
4,standard,low,domestic
5,standard,low,international

# Stage 2: Rules determine actions
@free shipping,rule
y,1
y,2
n,3
n,4
n,5

@apply discount,rule
n,1
y,2
y,3
n,4
n,5

@add customs fee,rule
n,1
n,2
y,3
n,4
y,5
```

The dtc is not constrained by this pattern. Use (sub)tables to decompose your problem as desired.

### Cross-Domain Bridge Pattern

The decomposition above (small tables for human clarity) is *intra-domain*: one author splitting their own table for readability. A second, more powerful decomposition is *inter-domain*: two authors working in different vocabularies on the same problem.

Decision tables are a notation for domain experts. The names a domain expert chooses ("Eating?", "I'm hungry", "Order requires special HAZMAT") are exactly the names they use when reasoning about the problem — punctuation, spaces, question marks and all. These names are CSV-legal but hostile to most target programming languages.

A second author — the integrator generating code for a specific target — can write a small *bridge* table whose only purpose is to translate names across the boundary. The bridge is itself a `.dtc` file; no new mechanism is required.

**The load-bearing fact:** dtc just manipulates strings. It never parses, validates, or interprets the contents of a name or a value — they are opaque bytes that dtc only compares for equality. Every property of the pattern that follows comes from that. The `~` marker, the `%s` placeholder, the C operators, the languages on either side of the bridge — none of them mean anything to dtc; they are a contract between the bridge author and the *translator* that consumes the `.psu`.

**Convention:** the integrator chooses a single character not used in any domain name (e.g. `~`) and prefixes it on every name that should be visible at the target-language boundary. The bridge consists of trivial 1:1 rules connecting the two vocabularies. The post-`~` text can be either an *identifier* (the convenience form, below) or a literal *target-language template* (the general form, further down). dtc is unaware of the distinction; only the translator is.

Domain author writes `appetite.dtc`:
```csv
@"Eating?","I'm hungry","Food's here?"
yes,yes,yes
no,yes,no
no,no,
```

Integrator writes `appetiteToC.dtc`:
```csv
# Map C-side inputs INTO domain inputs.
@"I'm hungry",~hungry
yes,yes
no,no

@"Food's here?",~food
yes,yes
no,no

# Map domain output OUT to C-side output.
@~eat,"Eating?"
yes,yes
no,no
```

Co-compile:
```bash
./dtc appetite.dtc appetiteToC.dtc > appetite.psu
```

**The I/O metadata partitions itself.** Look at the resulting `.psu` header:

```csv
I,~food,no
I,~food,yes
I,~hungry,no
I,~hungry,yes
O,Eating?,no
O,Eating?,yes
O,Food's here?,no
O,Food's here?,yes
O,I'm hungry,no
O,I'm hungry,yes
O,~eat,no
O,~eat,yes
```

Three groups have appeared without anyone tagging them:

- **`I,` lines are *only* the `~`-prefixed names** — the boundary inputs. The original domain inputs (`I'm hungry`, `Food's here?`) have been promoted to `O,` because the bridge resolves them.
- **`~`-prefixed `O,` lines are boundary outputs** — the values the caller will read.
- **Unprefixed `O,` lines are intermediates** — the trace of the domain author's reasoning, preserved through the optimization.

**The optimizer absorbs trivial bridges.** In the body of the bridged pseudocode, every `T,` test is on a `~`-prefixed boundary input — the optimizer sees through the 1:1 renamings and never tests an intermediate. The intermediate `R,` lines remain in the DAG, path-correct, as a faithful trace of the domain reasoning.

**Translator's job:**
- Function signature: every `I,` becomes a parameter, every `~`-prefixed `O,` becomes an output pointer; strip the `~` for the C identifier.
- For each `T,var,val,N`: emit a real C test (every `T` in a cleanly bridged DAG is on a boundary input).
- For each `R,var,val`: if `var` is `~`-prefixed, emit the C assignment; otherwise emit a comment carrying the original domain text (e.g. `/* "Eating?" = yes */`) at that exact decision-tree leaf, so the generated code documents itself in the spec's vocabulary.
- Enums for unprefixed `O,` names are internal — keep them out of the public `.h`.

```c
void appetiteEvaluate(
  enum appetite_food_e food
 ,enum appetite_hungry_e hungry
 ,enum appetite_eat_e *eat
){
  if (food == appetite_food_no)
    goto L1;
  /* "Food's here?" = yes */
  if (hungry == appetite_hungry_no)
    goto L3;
  /* "Eating?" = yes */
  /* "I'm hungry" = yes */
  *eat = appetite_eat_yes;
  goto L0;
L3:
  /* "Eating?" = no */
  /* "I'm hungry" = no */
  *eat = appetite_eat_no;
  goto L0;
  ...
}
```

The C side reads as clean C; the domain vocabulary survives as comments at the exact decision-tree leaves where each rule fires. A non-trivial bridge (multiple boundary inputs combining into one domain input) may leave an intermediate that *is* tested by the optimizer; in that case the translator emits a local variable for it and the comments still apply.

**Why this is correct:** dtc's optimizer resolves every output (boundary or intermediate) at every leaf according to the consistent inputs of the path, so intermediate `R,` values are reliable. Multi-table chaining through the bridge is what `dtc.c`'s transitive closure (commit `32d33cd`) makes possible — without it, multi-chain inferences are not reachable and the bridged DAG comes out empty.

#### Template Form (general)

The identifier-style bridge above forces the translator to *synthesize* C around the boundary symbols (the operator `==`, the assignment `=`, the wrapping `void evaluate(...)` signature, the namespaced enums, the header file). The translator has to know it is producing C.

Pushed all the way, the convention can do better: the post-`~` text is itself a literal target-language *template* containing a `%s` placeholder, and the translator becomes a `printf`-style substituter. The bridge author writes the target syntax verbatim; dtc treats it as opaque text; the translator only fills in the value at `%s`.

The two emission rules collapse to:

- `T,~<test-template>,<value>,<label>` → emit `sprintf(<test-template>, <value>)` followed by `\n  goto L<label>;`
- `R,~<resolve-template>,<value>` → emit `sprintf(<resolve-template>, <value>)`

Bridge written in template form:

```csv
# Test templates — operator, parens, everything; the value fills %s.
@"I'm hungry","~if (hungry == %s)"
yes,YES
no,NO

@"Food's here?","~if (food == %s)"
yes,YES
no,NO

# Resolve templates — LHS, operator, terminator; the value fills %s.
@"~eat = %s;","Eating?"
YES,yes
NO,no
```

The translator emits a snippet that drops directly into whatever enclosing function/case-block/loop body the integrator chose:

```c
  if (food == NO)
    goto L1;
  /* "Food's here?" = yes */
  if (hungry == NO)
    goto L3;
  /* "Eating?" = yes */
  /* "I'm hungry" = yes */
  eat = YES;
  goto L0;
L3:
  /* "Eating?" = no */
  /* "I'm hungry" = no */
  eat = NO;
  goto L0;
  ...
```

No function signature, no header, no enums, no name mangling — the translator never had to know about C as a language. The bridge author chose every character. The same machinery emits a `switch`-arm body, a `while`-loop body, a Python block, an assembly snippet, or a SQL `CASE` expression by changing only the bridge file. `awk`'s built-in `printf` is the entire mechanism; `C.awk` minus the enum/signature/identifier-mangling code is what a template-aware translator looks like.

**Trade-offs vs identifier form:**
- Template form gives the integrator total syntactic control and minimal translator complexity, but the bridge file carries the target syntax explicitly and the emitted snippet must be valid in the enclosing context the integrator chose.
- Identifier form gives a self-contained `.h`/`.c` pair with type-safe enums and a callable function, at the cost of a translator that knows the target language.
- Both forms can coexist in the same bridge — use identifier form for symbols that fit the convenience pattern, template form for the ones that don't.

**Why this matters:**
- The domain expert is never forced into the target language's identifier rules.
- The integrator never edits the domain table to make it compile.
- Both sides write only `.dtc`; the joint optimization is the whole pipeline.
- The same domain table can be bridged to multiple targets — `appetiteToC.dtc`, `appetiteToPy.dtc`, etc. — by different integrators using their own prefix conventions, without any change to `appetite.dtc`.

**What stays out of the bridge.** Not every target-side condition wants to be bridged. The optimization DTC offers is *test re-ordering* — finding the depth-minimal way to evaluate a set of predicates. A trivial guard the integrator can hand-order in one line (a null-pointer check, a range compare, a single-bit test, a dedup) has no ordering insight to expose; bridging it anyway adds a boundary-input dimension that multiplies the search space, replaces a single C variable with a single-use boolean local, and removes the integrator's fast early-exit return path. The bridge earns its keep on predicates whose evaluation order is non-obvious, or whose cost varies enough that ordering matters (function-call results vs bit tests). Static rules of a published algorithm where multiple predicates feed multiple outputs with overlap are the sweet spot; trivial structural guards belong in target-language code.

### Multi-File Compilation

The compiler merges all tables from all input files, solving them together into a single optimized DAG. This enables a "multiple sheets" decomposition style familiar to spreadsheet users:

```bash
# Compile tables from multiple files together
./dtc conditions.dtc actions.dtc overrides.dtc > combined.psu
```

**Use cases:**
- **Organizational separation**: Group related tables by domain concern (e.g., pricing.dtc, shipping.dtc, discounts.dtc)
- **Team workflows**: Different domain experts maintain different files
- **Conditional inclusion**: Include or exclude table files based on configuration

**Important consideration:**
Tables with interdependencies (shared names) benefit from joint optimization. However, compiling independent tables together increases optimization time exponentially since the search space combines. For independent decision problems:

```bash
# Independent tables: compile separately (faster)
./dtc power.dtc > power.psu
./dtc driving.dtc > driving.psu

# Interdependent tables: compile together (better optimization)
./dtc proceed.dtc brake.dtc accelerator.dtc > driving.psu
```

If compilation takes too long, consider whether your tables are truly interdependent. (Review the pseudocode metadata that identifies inputs, the values the dtc has determined are independent.) Splitting independent problems into separate compilation units reduces optimization time while producing equivalent results.

## Output Format

The pseudocode output is in CSV format, making it easy to parse in any language. Each line is a CSV record with the operation type in the first field.

**Example output for the traffic light decision table, DisjunctiveNormalForm.dtc:**
```csv
I,canStop,no
I,canStop,yes
I,isClose,no
I,isClose,yes
I,signal,green
I,signal,red
I,signal,yellow
O,accelerator,no
O,accelerator,yes
O,brake,no
O,brake,yes
O,proceed,no
O,proceed,yes
D,3
T,signal,yellow,1
L,2
T,signal,green,3
L,4
R,accelerator,no
R,brake,yes
R,proceed,no
J,0
L,3
R,brake,no
R,proceed,yes
T,isClose,no,5
L,6
R,accelerator,yes
J,0
L,5
R,accelerator,no
J,0
L,1
T,canStop,no,3
J,4
L,0
```

### Pseudocode Syntax

**Metadata Lines:**
- **`I,var,val`** - Input (independent) name and value - provides type information
- **`O,var,val`** - Output (dependent) name and value - provides type information
- **`D,n`** - Depth (maximum decision depth, worst-case tests to reach a leaf) - complexity metric

**Code Lines:**
- **`L,n`** - Label definition (numeric, 0 is exit)
- **`T,var,val,n`** - Test: if var equals val, jump to label n
- **`J,n`** - Jump unconditionally to label n (0 = exit/return)
- **`R,var,val`** - Resolve: assign val to var

**CSV Encoding:**
All names and values are CSV-encoded. Values containing commas, quotes, or newlines are quoted per RFC 4180:
```csv
I,"Order requires special ""HAZMAT""",y
R,"Add $17.50 foreign shipping fee",y
```

**Metadata Purpose:**
The metadata lines enable automatic generation of:
- Type-safe code with enums in statically-typed languages
- Function signatures with correct parameter types
- Visual diagrams (UML, flowcharts) without parsing code
- Documentation showing all possible values for each name

This metadata makes the pseudocode self-describing and sufficient for translation to any programming language or visualization format.

## Language Translation

The CSV pseudocode is designed to be easily parsed CSV text. The metadata (I and O lines) provides all information needed to generate type definitions and function signatures.

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

This example generates complete C code with:
- Separate enum for each variable with namespaced values
- Header file with type definitions and function declaration
- Implementation file with goto-based decision logic preserving DAG structure
- All identifiers prefixed with table name to avoid conflicts
- Special characters in names converted to underscores for valid C identifiers

### State Machine Translation

Languages without goto require translation to a [state machine](https://en.wikipedia.org/wiki/Finite-state_machine) pattern. This adds dispatch overhead but maintains the DAG structure and compact representation:

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

The decision table compiler generates a DAG where multiple decision paths converge at shared nodes. This structure:

1. **Eliminates duplicate code** - Tree-structured if/else would duplicate downstream logic at every branch point (exponential bloat)
2. **Maps to machine code** - goto translates directly to CPU jump instructions
3. **Avoids nesting limits** - Flat structure prevents deeply nested if/else hitting compiler limits
4. **Enables sharing** - Multiple paths to same outcome share the leaf node code

Example: In `DisjunctiveNormalForm.psu`, multiple decision paths converge to common tail code.

## C Code Generation

The example `C.awk` script translates CSV pseudocode into complete C header and implementation files with proper type safety and namespace isolation.

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

### Identifier Conversion

Variable names and values are converted to valid C identifiers:
- Spaces become underscores: `Favored customer` → `Favored_customer`
- Special characters become underscores: `Total order amount > $500.00` → `Total_order_amount____500_00`
- Leading digits get underscore prefix: `1stChoice` → `_1stChoice`

## Python Code Generation

The example `psu2py.py` script translates CSV pseudocode into Python modules using a state machine pattern.

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
```csv
I,signal,green
I,signal,red
O,proceed,no
O,proceed,yes
D,2
T,signal,green,1
L,2
R,proceed,no
J,0
L,1
R,proceed,yes
J,0
L,0
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

### Identifier Conversion

Like C.awk, psu2py.py converts names to valid Python identifiers using the same rules (special characters become underscores).

## Visualization and Documentation

The pseudocode metadata provides complete information for generating visual representations of decision logic.

### Flowcharts and UML Diagrams

The metadata (I, O, D) combined with the pseudocode structure contains all necessary information to generate:

**UML Activity Diagrams:**
- Decision nodes (diamonds) from `T,var,val,n` conditionals
- Action nodes (rectangles) from `R,var,val` assignments
- Control flow from `J,n` jumps
- Entry point from start of code (before first label)
- Exit point from label 0 or `J,0` jumps
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
- All names and their possible values (I, O)
- Complete control flow graph (labels and jumps)
- Decision conditions with explicit comparisons
- Complexity metric (D) for documentation
- Entry and exit points clearly marked

This allows domain experts to review decision logic visually while maintaining a single source of truth (the .dtc file).

## Computational Complexity

The decision table optimization problem is [NP-complete](https://en.wikipedia.org/wiki/NP-completeness). Finding the optimal decision tree that minimizes worst-case evaluation depth while maximizing node sharing involves exploring an exponentially large search space.

### Compilation Time

**Compilation time varies based on:**
- **Number of input names**: Each additional name exponentially increases the search space
- **Number of unique values per name**: More values = more possible test orderings

**Practical expectations:**
- **Simple tables** (3-5 independent values): Milliseconds
- **Medium tables** (8-10 independent values): Seconds
- **Complex tables** (15+ independent values): Minutes or longer

### Why Optimization Matters

**Without optimization**, a naive implementation might:
- Test every condition sequentially (maximum depth)
- Duplicate code at every branch point (exponential size)

**With dtc optimization**, the `power.dtc` example (10 input names, 20 values):
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
