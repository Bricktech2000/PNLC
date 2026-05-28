# PNLC

_A toy functional language_

PNLC consists of two components:

1. [An interpreter](pnlc.c) for a lazily-evaluated untyped λ‑calculus augmented with I/O.
2. [A prelude](prelude.pnlc) that bootstraps the calculus into a practical language.

## The Interpreter

A PNLC program is a λ‑term written in prefix notation. Prefix notation simplifies parsing and obviates a syntax for `let` bindings. The grammar is specified in [grammar.bnf](grammar.bnf). Roughly speaking,

```bnf
<term> ::= "." <term> <term> ; application
         | "\\" <var> <term> ; abstraction
         | <var>             ; variable
```

I/O is based on continuations. During execution, the top-level term is expected to β‑reduce to one of the following forms, at which point the corresponding effect is performed.

| Form                | Effect                                                                                                                              |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `$exit`             | Terminate the program normally.                                                                                                     |
| `$err …`            | Ignore any arguments, crash the program.                                                                                            |
| `.$get cont`        | Read one bit from `stdin`, invoke `cont` with it. `\s \n n` means EOF, `\s \n .s \t \f t` means one, `\s \n .s \t \f f` means zero. |
| `..$put bit cont`   | Write `bit` to `stdout`, invoke `cont` without arguments. A bit of `\t \f t` means one, `\t \f f` means zero.                       |
| `..$eput bit cont`  | Write `bit` to `stderr`, invoke `cont` without arguments. A bit of `\t \f t` means one, `\t \f f` means zero.                       |
| `..$dump term cont` | Dump `term` to `stderr`, invoke `cont` without arguments.                                                                           |

## The Prelude

The prelude defines data types like booleans, integers, pairs, optionals, lists, characters, strings; type classes like monoids, functors, monads, comonads, foldables; type class instances like monoids under addition, total ordering of lists, the stream functor, the I/O monad; and more.

Arguably, the prelude _is_ the language, because without it you’re left with little more than a λ‑calculus interpreter. Much of it is inspired by Haskell’s base library and Miranda’s standard environment. As a first step in becoming familiar with it you might study the [example programs](examples/) included—though not the “naked” ones, because they’re designed not to depend on the prelude.

## Examples

First, compile the interpreter:

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
bin/pnlc prelude.pnlc examples/rule\ 110.pnlc
bin/pnlc prelude.pnlc examples/fizzbuzz.pnlc
bin/pnlc prelude.pnlc examples/hanoi.pnlc
bin/pnlc prelude.pnlc examples/fib.pnlc
bin/pnlc prelude.pnlc examples/ack.pnlc
bin/pnlc io\ hook.pnlc prelude.pnlc examples/hello\ world.pnlc
bin/pnlc io\ hook.pnlc prelude.pnlc examples/bit-cat.pnlc

# PNLC self-interpreter demos
bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat examples/no-op\ naked.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat examples/hello\ world\ naked.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat examples/bit-cat\ naked.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat prelude.pnlc examples/ack.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat prelude.pnlc examples/fib.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat prelude.pnlc examples/hanoi.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat prelude.pnlc examples/fizzbuzz.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat prelude.pnlc examples/greeting.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat prelude.pnlc examples/pnlc.pnlc nul examples/no-op\ naked.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat prelude.pnlc examples/pnlc.pnlc nul examples/hello\ world\ naked.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat prelude.pnlc examples/pnlc.pnlc nul examples/bit-cat\ naked.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat io\ hook.pnlc examples/hello\ world\ naked.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat io\ hook.pnlc examples/bit-cat\ naked.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc.pnlc

# regex engine demos
bin/pnlc prelude.pnlc examples/regex.pnlc
cat examples/re/comment.re pnlc.c | bin/pnlc prelude.pnlc examples/regex.pnlc
cat examples/re/strlit.re pnlc.c | bin/pnlc prelude.pnlc examples/regex.pnlc
cat examples/re/utf-8.re README.md | bin/pnlc prelude.pnlc examples/regex.pnlc
cat examples/re/divby3.re examples/rule\ 110.pnlc | bin/pnlc prelude.pnlc examples/regex.pnlc
cat examples/re/ipv4.re examples/regex.pnlc | bin/pnlc prelude.pnlc examples/regex.pnlc
cat examples/re/hexrgb.re examples/regex.pnlc | bin/pnlc prelude.pnlc examples/regex.pnlc

# Brainfuck interpreter demos
bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/pnlc.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/bell.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/ascii.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/cat.bf bang - | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/reverse.bf bang - | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/beaver.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/clear.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/head.bf bang - | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/strip.bf bang - | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/truth-machine.bf bang - | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/bin2text.bf bang - | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/text2bf.bf bang - | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/fib.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/squares.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/thuemorse.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/sierpinski.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/bf/siercarpet.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
```
