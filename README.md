# PNLC

_A toy pure functional language_

PNLC is λ‑calculus in prefix notation plus normal-order semantics plus continuation-based I/O plus a small prelude.

```bnf
<term> ::= "." <term> <term> ; application
         | "\\" <var> <term> ; abstraction
         | <var>             ; variable
```

<!-- keep in sync with pnlc.c, pnlc.vim and io\ hook.pnlc -->

| Top-Level Term      | Effect                                                                                                                              |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `$exit`             | Terminate execution.                                                                                                                |
| `$err …`            | Ignore any arguments, error out, terminate execution.                                                                               |
| `.$get cont`        | Read one bit from `stdin`, invoke `cont` with it. `\s \n n` means EOF, `\s \n .s \t \f t` means one, `\s \n .s \t \f f` means zero. |
| `..$put bit cont`   | Write `bit` to `stdout`, invoke `cont` without arguments. A bit of `\t \f t` means one, `\t \f f` means zero.                       |
| `..$dump term cont` | Reduce `term` to weak head normal form, dump it to `stderr`, invoke `cont` without arguments.                                       |

To run some example programs:

```sh
make bin/pnlc
bin/pnlc examples/raw-cat.pnlc
bin/pnlc prelude.pnlc examples/bit-cat.pnlc
bin/pnlc prelude.pnlc examples/chr-cat.pnlc
bin/pnlc prelude.pnlc examples/truth-machine.pnlc
bin/pnlc prelude.pnlc examples/greeting.pnlc
bin/pnlc prelude.pnlc examples/reverse.pnlc
bin/pnlc prelude.pnlc examples/rot13.pnlc
bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/pnlc.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/beaver.bf | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
cat examples/truth-machine.bf - | bin/pnlc prelude.pnlc examples/brainfuck.pnlc
bin/pnlc io\ hook.pnlc prelude.pnlc examples/bit-cat.pnlc
```
