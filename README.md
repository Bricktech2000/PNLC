# PNLC

_A toy functional language_

## Overview

PNLC is λ‑calculus in prefix notation plus normal-order semantics plus continuation-based I/O plus a small prelude. Arguably, [the prelude](prelude.pnlc) _is_ the language, because without it you’re left with little more than [a λ‑calculus interpreter](pnlc.c).

The grammar for the language is specified in [grammar.bnf](grammar.bnf). Roughly speaking,

```bnf
<term> ::= "." <term> <term> ; application
         | "\\" <var> <term> ; abstraction
         | <var>             ; variable
```

During execution, the top-level term is expected to β‑reduce to one of the following forms, at which point the corresponding effect is performed. The prelude provides monadic wrappers for use in user programs.

<!-- keep in sync with pnlc.c, examples/pnlc.pnlc, pnlc.vim and io\ hook.pnlc -->

| Form                | Effect                                                                                                                              |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `$exit`             | Terminate the program normally.                                                                                                     |
| `$err …`            | Ignore any arguments, crash the program.                                                                                            |
| `.$get cont`        | Read one bit from `stdin`, invoke `cont` with it. `\s \n n` means EOF, `\s \n .s \t \f t` means one, `\s \n .s \t \f f` means zero. |
| `..$put bit cont`   | Write `bit` to `stdout`, invoke `cont` without arguments. A bit of `\t \f t` means one, `\t \f f` means zero.                       |
| `..$eput bit cont`  | Write `bit` to `stderr`, invoke `cont` without arguments. A bit of `\t \f t` means one, `\t \f f` means zero.                       |
| `..$dump term cont` | Reduce `term` to weak head normal form, dump it to `stderr`, invoke `cont` without arguments.                                       |

## Usage

First compile the interpreter:

```sh
make bin/pnlc
```

Then run some example programs:

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
bin/pnlc io\ hook.pnlc prelude.pnlc examples/hello\ world.pnlc
bin/pnlc io\ hook.pnlc prelude.pnlc examples/bit-cat.pnlc

# PNLC self-interpreter demos
bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat examples/no-op\ naked.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat examples/hello\ world\ naked.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat examples/bit-cat\ naked.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat io\ hook.pnlc examples/hello\ world\ naked.pnlc | bin/pnlc prelude.pnlc examples/pnlc.pnlc
cat io\ hook.pnlc examples/bit-cat\ naked.pnlc nul - | bin/pnlc prelude.pnlc examples/pnlc.pnlc

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
