# IFJ interpreter

This is a tree-walking interpreter for a small C-like language from the IFJ course at FIT VUT. It started as an unfinished university project (scanner notes and a grammar draft) and was finished with AI assistance: parser, AST, semantics, runtime, tests, and the memory model below.

A program is scanned, parsed into one abstract syntax tree, checked for a valid `main`, then executed by walking that tree. There is no bytecode. The tree is the intermediate representation.

## Language

Types are `int`, `double`, `string`, and `auto`. `auto` takes the type of its initializer. Using `auto` as a function return type or parameter type is an error. There are no arrays, pointers, or user-defined types.

A source file is a list of functions. A function may be declared with `;` and defined later. `main` must exist, must have a body, and must take no parameters. Definitions are global; functions cannot be nested.

Statements are blocks, the empty statement `;`, variable declarations, `if` / `else`, `for`, `while`, `do` / `while`, `return`, `cin`, `cout`, assignment, and calls. `if` and loops take a statement or a block. The `for` header may omit the initializer, the condition, or the step. The step, when present, is only `id = expression`.

Expressions follow C-like precedence: unary `-` and `!`, then `* / %`, `+ -`, relations, `==` and `!=`, `&&`, `||`. `&&` and `||` skip the right operand when the left operand already decides the result. `+` on two strings concatenates. Numeric types mix `int` and `double` by promoting to `double`.

Built-ins, all operating on strings:

| Function | Meaning |
| --- | --- |
| `length(s)` | character count |
| `substr(s, i, n)` | slice starting at `i` |
| `concat(a, b)` | new string |
| `find(s, pat)` | first index of `pat`, or `-1` |
| `sort(s)` | new string, characters heap-sorted |

`find` uses Knuth–Morris–Pratt. `sort` uses heapsort.

`throw expression;` raises a value. The value must be an initialized string. `catch (string name)` handles the nearest explicit throw. Division by zero, a bad `cin` conversion, and other runtime faults are not catchable. An uncaught throw exits with code 10.

The full grammar is in `docs/grammar.txt`.

## How a run works

```
source → scanner → parser → AST → sem_prepare → tree walk → exit code
```

`main` owns one `compiler_t`: input file, scanner, current token, program node, and a list of every AST node allocated for that run. The scanner (`lexal.c`) yields one token at a time. Identifiers and string literals own a heap copy of their text. Keywords are case-sensitive.

The parser (`syntal.c`) is recursive descent. Statements are LL(1). Expressions are precedence climbing. Each node is registered on the compiler as it is created, so a syntax error can still free nodes that never got linked into the tree.

If the parse succeeds, `interpret` builds an `interp_t`, registers the five built-ins, then `sem_prepare` (`sem.c`) enters every function into a hash table. A second definition, a clash with a built-in name, a missing `main`, or a `main` with parameters fails before execution.

Execution (`interp.c`) is one recursive walk. Each call returns a completion: normal, return, or throw. A return stops at the function that produced it. A throw walks out through blocks, loops, and calls until a `try` catches it or `main` ends. That is ordinary C control flow. `setjmp` is not used, because it would skip the stack and string cleanup on the way out.

The first error is recorded on the compiler and later calls keep that code. `main` always runs `compiler_cleanup`, then returns the status. Allocation failure still aborts the process with code 99.

Exit codes match the course numbering:

| Code | Meaning |
| --- | --- |
| 0 | success |
| 1 | lexical error |
| 2 | syntax error |
| 3 | semantic error (undefined name, missing `main`, duplicate function) |
| 4 | type error |
| 5 | `auto` could not be deduced |
| 6 | other semantic error (`auto` in a signature, too many arguments) |
| 7 | bad numeric input |
| 8 | use of an uninitialized value |
| 9 | division by zero |
| 10 | other runtime error, including an uncaught throw |
| 99 | internal failure, including out of memory |

Many type and initialization checks happen when that statement actually runs. A bad expression on a path the program never takes does not fail the run.

## Data model

Every AST node is the same `ast_t`. The kind selects which of `a`, `b`, `c`, `d`, and `next` are meaningful. A block's statements are a `next` list. Try and throw go through named accessors (`ast_try_body`, `ast_try_handler`, `ast_throw_expr`); the other kinds use the fields directly.

Runtime values sit on one growable object stack of fixed `value_t` slots:

```
type, initialized flag, int, double, char *string
```

A slot index stays valid when the stack is reallocated. The symbol table stores `index + 1` as a `void *`, so a missing entry and slot 0 stay distinct. Strings are not inline. The slot holds an owning `char *`, allocated separately, and copied on assignment.

A frame records the stack index where its locals begin, plus a hash table from names to slots. Leaving a block rewinds the stack to that index and destroys string payloads above it. A return or throw lifts its value out, rewinds, then pushes the value back. A function frame has no parent, so `return` does not leak into the caller. The caller saves its own frame pointer and restores it after the call.

The hash table (`ial.c`) is chained buckets. It holds functions for the whole program and variables for each frame. Function entries point at AST nodes they do not own. The compiler's node list owns the tree.

## Build and tests

```
make            # build/ifj
make test       # tests/*.src against tests/*.out and optional *.status
make memcheck   # same suite under Valgrind, including expected failures
make clean
```

```
./build/ifj program.src
./build/ifj < program.src
```

`tests/sieve.src` is the Sieve of Eratosthenes up to 20,000. The flag table is a string of `"1"` and `"0"`. It prints `2262`.

## Downsides

The walk interprets the tree directly, so every operator pays for a recursive call and a stack slot. There is no constant folding and no compiled loop.

Strings are immutable values with eager copies. Updating one character rebuilds the whole string with `substr` and `concat`. The sieve is therefore quadratic in the limit, which is a property of this representation, not of the algorithm.

The single node type makes the tree cheap to allocate and awkward to read. A wrong child pointer is a silent logic bug. Only try and throw have accessors.

Semantic analysis before execution covers function identity and `main`. Types of expressions, definite initialization, and argument counts are checked during the walk, and only for code that runs.

`xmalloc` still exits on allocation failure, so that path skips `compiler_cleanup`. Language errors do not.

The object stack, frames, and the big statement switch still live in one interpreter file. Built-ins and the function table are split out; the execution engine is not.
