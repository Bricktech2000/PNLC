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

Much of the prelude is inspired by Haskell’s base library and Miranda’s standard environment. As a first step in becoming familiar with it you might study the [example programs](examples/) included—though not the “naked” ones, because they’re designed not to depend on the prelude.

## Examples

First, compile the translator:

```sh
make bin/pnlc
```

Then, run any of the example programs:

```sh
bin/pnlc examples/no-op\ naked.pnlc
bin/pnlc examples/hello\ world\ naked.pnlc
bin/pnlc examples/bit-cat\ naked.pnlc
bin/pnlc prelude.pnlc examples/no-op.pnlc
bin/pnlc prelude.pnlc examples/hello\ world.pnlc
bin/pnlc prelude.pnlc examples/bit-cat.pnlc
bin/pnlc prelude.pnlc examples/chr-cat.pnlc
bin/pnlc prelude.pnlc examples/greeting.pnlc
bin/pnlc prelude.pnlc examples/truth-machine.pnlc
bin/pnlc prelude.pnlc examples/reverse.pnlc
bin/pnlc prelude.pnlc examples/rot13.pnlc
bin/pnlc prelude.pnlc examples/quine.pnlc
bin/pnlc prelude.pnlc examples/yin-yang.pnlc
bin/pnlc prelude.pnlc examples/rule\ 110.pnlc
bin/pnlc prelude.pnlc examples/fizzbuzz.pnlc
bin/pnlc prelude.pnlc examples/hanoi.pnlc
bin/pnlc prelude.pnlc examples/ack.pnlc
bin/pnlc prelude.pnlc examples/primes.pnlc
bin/pnlc prelude.pnlc examples/fib\ dec.pnlc
bin/pnlc prelude.pnlc examples/fib\ bin.pnlc
bin/pnlc prelude.pnlc examples/collatz\ dec.pnlc
bin/pnlc prelude.pnlc examples/collatz\ bin.pnlc
bin/pnlc hook prelude.pnlc examples/hello\ world.pnlc
bin/pnlc hook prelude.pnlc examples/bit-cat.pnlc
bin/pnlc prelude.pnlc emit examples/greeting.pnlc && make bin/emit && bin/emit
bin/pnlc prelude.pnlc emit examples/fib\ dec.pnlc && make bin/emit && bin/emit

# PNLC self-interpreter demos
bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
bin/pnlc prelude.pnlc examples/pnlc\ metacircular.pnlc
bin/pnlc prelude.pnlc examples/pnlc\ shallow.pnlc
cat examples/no-op\ naked.pnlc         | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat examples/hello\ world\ naked.pnlc  | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat examples/bit-cat\ naked.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/primes.pnlc         | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/fib\ dec.pnlc       | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/ack.pnlc            | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/hanoi.pnlc          | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/fizzbuzz.pnlc       | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/greeting.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/pnlc\ definitional.pnlc nul examples/no-op\ naked.pnlc         | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/pnlc\ definitional.pnlc nul examples/hello\ world\ naked.pnlc  | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat prelude.pnlc examples/pnlc\ definitional.pnlc nul examples/bit-cat\ naked.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat hook examples/hello\ world\ naked.pnlc  | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
cat hook examples/bit-cat\ naked.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc\ definitional.pnlc
bin/pnlc prelude.pnlc emit examples/pnlc\ definitional.pnlc && make bin/emit && cat prelude.pnlc examples/greeting.pnlc nul - | bin/emit
bin/pnlc prelude.pnlc emit examples/pnlc\ definitional.pnlc && make bin/emit && cat prelude.pnlc examples/fib\ dec.pnlc       | bin/emit

# regex engine demos
bin/pnlc run prelude.pnlc examples/regex.pnlc
cat examples/re/comment.re pnlc.c                  | bin/pnlc run prelude.pnlc examples/regex.pnlc
cat examples/re/strlit.re  pnlc.c                  | bin/pnlc run prelude.pnlc examples/regex.pnlc
cat examples/re/utf-8.re   README.md               | bin/pnlc run prelude.pnlc examples/regex.pnlc
cat examples/re/divby3.re  examples/rule\ 110.pnlc | bin/pnlc run prelude.pnlc examples/regex.pnlc
cat examples/re/ipv4.re    examples/regex.pnlc     | bin/pnlc run prelude.pnlc examples/regex.pnlc
cat examples/re/hexrgb.re  examples/regex.pnlc     | bin/pnlc run prelude.pnlc examples/regex.pnlc
bin/pnlc prelude.pnlc emit examples/regex.pnlc && make bin/emit && cat examples/re/comment.re pnlc.c | bin/emit
bin/pnlc prelude.pnlc emit examples/regex.pnlc && make bin/emit && cat examples/re/strlit.re  pnlc.c | bin/emit
cat examples/re/comment.re | bin/pnlc prelude.pnlc examples/regex.pnlc && make bin/emit && cat pnlc.c | bin/emit
cat examples/re/strlit.re  | bin/pnlc prelude.pnlc examples/regex.pnlc && make bin/emit && cat pnlc.c | bin/emit

# Brainfuck interpreter demos
bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/pnlc.bf                 | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/bell.bf                 | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/ascii.bf                | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/cat.bf bang -           | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/reverse.bf bang -       | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/beaver.bf               | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/clear.bf                | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/head.bf bang pnlc.c     | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/wc.bf bang Makefile     | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/truth-machine.bf bang - | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/collatz.bf bang -       | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/fib.bf                  | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/squares.bf              | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/thuemorse.bf            | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/sierpinski.bf           | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/siercarpet.bf           | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/ascii.bf bang           | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/cat.bf bang -           | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/reverse.bf bang -       | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/head.bf bang pnlc.c     | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/dbfi.bf bang examples/bf/truth-machine.bf bang - | bin/pnlc run prelude.pnlc examples/brainfuck.pnlc
bin/pnlc prelude.pnlc emit examples/brainfuck.pnlc && make bin/emit && cat examples/bf/head.bf bang pnlc.c | bin/emit
bin/pnlc prelude.pnlc emit examples/brainfuck.pnlc && make bin/emit && cat examples/bf/fib.bf              | bin/emit
cat examples/bf/head.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc && make bin/emit && cat pnlc.c | bin/emit
cat examples/bf/fib.bf  | bin/pnlc prelude.pnlc examples/brainfuck.pnlc && make bin/emit &&              bin/emit

# Forth DSL demos
bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/cat.f
bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/rot13.f
bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/hello\ world.f
bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/truth-machine.f
bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/fib.f
bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/collatz.f
bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/greeting.f
cat examples/bf/ascii.bf                | bin/pnlc run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/cat.bf bang -           | bin/pnlc run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/reverse.bf bang -       | bin/pnlc run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/head.bf bang pnlc.c     | bin/pnlc run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/truth-machine.bf bang - | bin/pnlc run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
cat examples/bf/thuemorse.bf            | bin/pnlc run prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f
bin/pnlc prelude.pnlc emit examples/forth.pnlc examples/f/brainfuck.f && make bin/emit && cat examples/bf/head.bf bang pnlc.c | bin/emit
bin/pnlc prelude.pnlc emit examples/forth.pnlc examples/f/brainfuck.f && make bin/emit && cat examples/bf/thuemorse.bf        | bin/emit
cat examples/bf/head.bf      | bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f && make bin/emit && cat pnlc.c | bin/emit
cat examples/bf/thuemorse.bf | bin/pnlc prelude.pnlc examples/forth.pnlc examples/f/brainfuck.f && make bin/emit &&              bin/emit
```
