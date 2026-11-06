# PNLC

_A small functional language_

PNLC consists of three components:

1. A lazily-evaluated untyped λ‑calculus augmented with I/O;
2. [A translator](pnlc.c) that can interpret and compile the calculus;
3. [A prelude](prelude.pnlc) that bootstraps the calculus into a practical language.

## The Calculus

A PNLC program is a lazily-evaluated λ‑term written in prefix notation. Prefix notation simplifies parsing and obviates syntax for `let` bindings or `where` clauses. The grammar is specified in [grammar.bnf](grammar.bnf). Roughly speaking,

```bnf
<term> ::= "." <term> <term> ; application
         | "\\" <var> <term> ; abstraction
         | <var>             ; variable
```

For example, _λf.(λx.f(xx))(λx.f(xx))_ is written `\f .\x .f .x x \x .f .x x`. There is no syntactic sugar apart from `#` comments. Binders shadow in the usual way, so `\x \x x` is α‑equivalent to `\x \y y`.

A well-formed program evaluates to a data structure that describes its I/O behavior, which a runtime then interprets. Conceptually it has the following shape:

```haskell
data Prog = Exit
          | Err
          | Emit Prog
          | Get (Maybe Bool -> Prog)
          | Put Bool Prog
          | Eput Bool Prog
          | Dump Term Prog
```

Operationally, the variables `$exit`, `$err`, `$emit`, `$get`, `$put`, `$eput`, `$dump` may occur free in a program’s λ‑term and are given the following reduction semantics:

| Top-Level Term      | Reduction                                                                                                                                       |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `$exit`             | Terminate normally.                                                                                                                             |
| `$err …`            | Discard any arguments and abort execution.                                                                                                      |
| `.$emit cont`       | Compile `cont` to an executable artifact and terminate normally. Alternatively, reduce to `cont` and proceed with execution.                    |
| `.$get cont`        | Read `bit` from `stdin` and reduce to `.cont bit`. A `bit` of `\s \n n` means EOF, `\s \n .s \t \f t` means one, `\s \n .s \t \f f` means zero. |
| `..$put bit cont`   | Write `bit` to `stdout` and reduce to `cont`. A `bit` of `\t \f t` means one, `\t \f f` means zero.                                             |
| `..$eput bit cont`  | Write `bit` to `stderr` and reduce to `cont`. A `bit` of `\t \f t` means one, `\t \f f` means zero.                                             |
| `..$dump term cont` | Dump an implementation-defined programmer-facing representation of `term` to `stderr` and reduce to `cont`.                                     |

## The Translator

The translator is an implementation of the calculus written in C99. It opens the files given as command-line arguments, concatenates their contents, and interprets the resulting program. If ever it encounters `.$emit cont` at the top level, it compiles `cont` to C99 source, writes it to ‘emit.c’ in the current working directory, and terminates. Subsequent calls to `$emit` arising from the compiled artifact are no-ops.

## The Prelude

The prelude defines data types like booleans, integers, pairs, optionals, lists, characters, strings; type classes like monoids, functors, monads, comonads, foldables; type class instances like monoids under addition, total ordering of lists, the stream functor, the I/O monad; and more.

The prelude is an integral part of the language because without it you’re left with little more than a λ‑calculus translator. This is by design: a small base language means simple implementations and tractable operational semantics; the calculus is a smallest language that, by means of a prelude, can still bootstrap itself into a practical, familiar functional language.

Much of the prelude is inspired by Haskell’s base library and Miranda’s standard environment. When a function is undocumented it's likely that it behaves the same as in Haskell or Miranda. As a first step in becoming familiar with the prelude you might study the [example programs](examples/) included—though not the “naked” ones, because they’re designed not to depend on the prelude.

## Examples

First, compile the translator:

```sh
make pnlc
```

Then, run any of the example programs:

```sh
# basic demos
# interpretation: PNLC programs interpreted by the translator
./pnlc examples/no-op\ naked.pnlc
./pnlc examples/hello\ world\ naked.pnlc
./pnlc examples/bit-cat\ naked.pnlc
./pnlc prelude.pnlc examples/no-op.pnlc
./pnlc prelude.pnlc examples/hello\ world.pnlc
./pnlc prelude.pnlc examples/bit-cat.pnlc
./pnlc prelude.pnlc examples/chr-cat.pnlc
./pnlc prelude.pnlc examples/head.pnlc
./pnlc prelude.pnlc examples/greeting.pnlc
./pnlc prelude.pnlc examples/truth-machine.pnlc
./pnlc prelude.pnlc examples/reverse.pnlc
./pnlc prelude.pnlc examples/rot13.pnlc
./pnlc prelude.pnlc examples/quine.pnlc
./pnlc prelude.pnlc examples/yin-yang.pnlc
./pnlc prelude.pnlc examples/rule\ 110.pnlc
./pnlc prelude.pnlc examples/fizzbuzz.pnlc
./pnlc prelude.pnlc examples/hanoi.pnlc
./pnlc prelude.pnlc examples/ack.pnlc
./pnlc prelude.pnlc examples/primes.pnlc
./pnlc prelude.pnlc examples/fib\ dec.pnlc
./pnlc prelude.pnlc examples/fib\ bin.pnlc
./pnlc prelude.pnlc examples/collatz\ dec.pnlc
./pnlc prelude.pnlc examples/collatz\ bin.pnlc
# same as above, but dumping the top-level term before executing each I/O action
./pnlc --hook prelude.pnlc examples/hello\ world.pnlc
./pnlc --hook prelude.pnlc examples/bit-cat.pnlc
# compilation: PNLC programs compiled by the translator
./pnlc prelude.pnlc --emit examples/greeting.pnlc && make emit && ./emit
./pnlc prelude.pnlc --emit examples/fib\ dec.pnlc && make emit && ./emit

# PNLC self-interpreter demos
./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
./pnlc prelude.pnlc examples/pnlc\ metacircular.pnlc
./pnlc prelude.pnlc examples/pnlc\ nbe.pnlc
./pnlc prelude.pnlc examples/pnlc\ shallow.pnlc
# interpretation from interpretation: PNLC programs interpreted by the self-interpreter interpreted by the translator
cat examples/no-op\ naked.pnlc         | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat examples/hello\ world\ naked.pnlc  | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat examples/bit-cat\ naked.pnlc nul - | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/primes.pnlc         | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/fib\ dec.pnlc       | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/ack.pnlc            | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/hanoi.pnlc          | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/fizzbuzz.pnlc       | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/greeting.pnlc nul - | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
# same as above, but dumping the top-level term before executing each I/O action
cat -- --hook examples/hello\ world\ naked.pnlc  | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat -- --hook examples/bit-cat\ naked.pnlc nul - | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
# self-interpretation from interpretation: PNLC programs interpreted by the self-interpreter interpreted by the self-interpreter interpreted by the translator
cat prelude.pnlc examples/pnlc\ definitional.pnlc nul examples/no-op\ naked.pnlc         | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/pnlc\ definitional.pnlc nul examples/hello\ world\ naked.pnlc  | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/pnlc\ definitional.pnlc nul examples/bit-cat\ naked.pnlc nul - | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
# compilation of the self-interpreter: PNLC programs interpreted by the self-interpreter compiled by the translator
./pnlc prelude.pnlc --emit examples/pnlc\ definitional.pnlc && make emit && cat prelude.pnlc examples/greeting.pnlc nul - | ./emit
./pnlc prelude.pnlc --emit examples/pnlc\ definitional.pnlc && make emit && cat prelude.pnlc examples/fib\ dec.pnlc       | ./emit

# PNLC self-compiler demos
cat -- --emit - | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
# compilation from interpretation: PNLC programs compiled by the self-compiler interpreted by the translator
cat -- --emit examples/no-op\ naked.pnlc        | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- --emit examples/hello\ world\ naked.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- --emit examples/bit-cat\ naked.pnlc      | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- prelude.pnlc --emit examples/primes.pnlc   | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- prelude.pnlc --emit examples/fib\ dec.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- prelude.pnlc --emit examples/ack.pnlc      | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- prelude.pnlc --emit examples/hanoi.pnlc    | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- prelude.pnlc --emit examples/fizzbuzz.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- prelude.pnlc --emit examples/greeting.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
# same as above, but dumping the top-level term before executing each I/O action
cat -- --emit --hook examples/hello\ world\ naked.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
cat -- --emit --hook examples/bit-cat\ naked.pnlc      | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && ./emit
# self-compilation from interpretation: PNLC programs compiled by the self-compiler compiled by the self-compiler interpreted by the translator
cat -- prelude.pnlc --emit examples/pnlc\ definitional.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && cat examples/no-op\ naked.pnlc         | ./emit
cat -- prelude.pnlc --emit examples/pnlc\ definitional.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && cat examples/hello\ world\ naked.pnlc  | ./emit
cat -- prelude.pnlc --emit examples/pnlc\ definitional.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && make emit && cat examples/bit-cat\ naked.pnlc nul - | ./emit
# compilation of the self-compiler: PNLC programs compiled by the self-compiler compiled by the translator
./pnlc prelude.pnlc --emit examples/pnlc\ definitional.pnlc && make emit && cat -- prelude.pnlc --emit examples/greeting.pnlc | ./emit 2> emit.c && make emit && ./emit
./pnlc prelude.pnlc --emit examples/pnlc\ definitional.pnlc && make emit && cat -- prelude.pnlc --emit examples/fib\ dec.pnlc | ./emit 2> emit.c && make emit && ./emit

# regex engine demos
./pnlc --run prelude.pnlc examples/regex.pnlc
# regular expressions run under the regex engine interpreted by the translator
cat examples/re/comment.re pnlc.c                  | ./pnlc --run prelude.pnlc examples/regex.pnlc
cat examples/re/strlit.re  pnlc.c                  | ./pnlc --run prelude.pnlc examples/regex.pnlc
cat examples/re/utf-8.re   README.md               | ./pnlc --run prelude.pnlc examples/regex.pnlc
cat examples/re/divby3.re  examples/rule\ 110.pnlc | ./pnlc --run prelude.pnlc examples/regex.pnlc
cat examples/re/ipv4.re    examples/regex.pnlc     | ./pnlc --run prelude.pnlc examples/regex.pnlc
cat examples/re/hexrgb.re  examples/regex.pnlc     | ./pnlc --run prelude.pnlc examples/regex.pnlc
# compilation of the regex engine: regular expressions run under the regex engine compiled by the translator
./pnlc prelude.pnlc --emit examples/regex.pnlc && make emit && cat examples/re/comment.re pnlc.c | ./emit
./pnlc prelude.pnlc --emit examples/regex.pnlc && make emit && cat examples/re/strlit.re  pnlc.c | ./emit
# compilation of regular expressions: {regular expressions run under the regex engine} compiled by the translator
cat examples/re/comment.re | ./pnlc prelude.pnlc examples/regex.pnlc && make emit && cat pnlc.c | ./emit
cat examples/re/strlit.re  | ./pnlc prelude.pnlc examples/regex.pnlc && make emit && cat pnlc.c | ./emit

# Brainfuck interpreter demos
./pnlc --run prelude.pnlc examples/brainfuck.pnlc
# interpretation of Brainfuck: Brainfuck programs interpreted by the PNLC Brainfuck interpreter interpreted by the translator
cat examples/bf/pnlc.bf                 | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/bell.bf                 | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/ascii.bf                | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/cat.bf bang -           | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/reverse.bf bang -       | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/beaver.bf               | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/clear.bf                | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/head.bf bang pnlc.c     | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/wc.bf bang Makefile     | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/truth-machine.bf bang - | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/collatz.bf bang -       | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/fib.bf                  | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/squares.bf              | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/thuemorse.bf            | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/sierpinski.bf           | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/siercarpet.bf           | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
# self-interpretation of Brainfuck: Brainfuck programs interpreted by a Brainfuck self-interpreter interpreted by the PNLC Brainfuck interpreter interpreted by the translator
cat examples/bf/dbfi.bf bang examples/bf/ascii.bf bang           | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/cat.bf bang -           | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/reverse.bf bang -       | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/head.bf bang pnlc.c     | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/truth-machine.bf bang - | ./pnlc --run prelude.pnlc examples/brainfuck.pnlc
# compilation of the Brainfuck interpreter: Brainfuck programs interpreted by the PNLC Brainfuck interpreter compiled by the translator
./pnlc prelude.pnlc --emit examples/brainfuck.pnlc && make emit && cat examples/bf/head.bf bang pnlc.c | ./emit
./pnlc prelude.pnlc --emit examples/brainfuck.pnlc && make emit && cat examples/bf/fib.bf              | ./emit
# compilation of Brainfuck programs: {Brainfuck programs interpreted by the PNLC Brainfuck interpreter} compiled by the translator
cat examples/bf/head.bf | ./pnlc prelude.pnlc examples/brainfuck.pnlc && make emit && cat pnlc.c | ./emit
cat examples/bf/fib.bf  | ./pnlc prelude.pnlc examples/brainfuck.pnlc && make emit &&              ./emit

# Forth DSL demos
# Forth programs run under the Forth DSL interpreted by the translator
./pnlc prelude.pnlc examples/forth.pnlc examples/f/cat.f
./pnlc prelude.pnlc examples/forth.pnlc examples/f/head.f
./pnlc prelude.pnlc examples/forth.pnlc examples/f/rot13.f
./pnlc prelude.pnlc examples/forth.pnlc examples/f/hello\ world.f
./pnlc prelude.pnlc examples/forth.pnlc examples/f/truth-machine.f
./pnlc prelude.pnlc examples/forth.pnlc examples/f/fib.f
./pnlc prelude.pnlc examples/forth.pnlc examples/f/collatz.f
./pnlc prelude.pnlc examples/forth.pnlc examples/f/greeting.f
# Brainfuck programs interpreted by the Forth Brainfuck interpreter run under the Forth DSL interpreted by the translator
cat examples/bf/ascii.bf                | ./pnlc --run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/cat.bf bang -           | ./pnlc --run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/reverse.bf bang -       | ./pnlc --run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/head.bf bang pnlc.c     | ./pnlc --run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/truth-machine.bf bang - | ./pnlc --run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/thuemorse.bf            | ./pnlc --run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
# compilation of the Forth Brainfuck interpreter: Brainfuck programs interpreted by the Forth Brainfuck interpreter run under the Forth DSL compiled by the translator
./pnlc prelude.pnlc --emit examples/forth.pnlc examples/f/brainfuck.f && make emit && cat examples/bf/head.bf bang pnlc.c | ./emit
./pnlc prelude.pnlc --emit examples/forth.pnlc examples/f/brainfuck.f && make emit && cat examples/bf/thuemorse.bf        | ./emit
# compilation of Brainfuck programs: {Brainfuck programs interpreted by the Forth Brainfuck interpreter run under the Forth DSL} compiled by the translator
cat examples/bf/head.bf      | ./pnlc prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f && make emit && cat pnlc.c | ./emit
cat examples/bf/thuemorse.bf | ./pnlc prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f && make emit &&              ./emit

# 'Reflections on Trusting Trust' demo (run in sequence)
make seed
make bootstrap
make bootstrap # ...
make INJECT='examples/trusting\ trust.pnlc' bootstrap
make bootstrap
make bootstrap # ...
cat prelude.pnlc examples/greeting.pnlc nul - | ./self
cat -- prelude.pnlc --emit examples/greeting.pnlc | ./self 2> emit.c && make emit && ./emit
# detection of a bugged compiler:
cat -- prelude.pnlc --emit examples/pnlc\ definitional.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> emit.c && diff -q -s self.c emit.c
```
