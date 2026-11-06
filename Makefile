.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: pnlc self
clean:; rm -f pnlc emit self emit.c self.orig.c
distclean: clean; rm -f self.c

# `$emit` under pnlc.c produces C code understood to serve as portable assembly
# language. when compiling it, optimize enough that the resulting machine code
# is reasonable, but not so much that the C compiler is carrying us.
EMIT=-O1 -fno-inline -fno-toplevel-reorder -foptimize-sibling-calls -fpeephole2 -fstore-merging
# '-foptimize-sibling-calls' is so `return func->u.beta(func, arg);` codegens to
#     mov    rax,QWORD PTR [rdi+0x8]
#     pop    ...
#     jmp    rax
# instead of
#     call   QWORD PTR [rax+0x8]
#     pop    ...
#     ret
# '-fpeephole2' is so `--self->refcount` codegens to
#     sub    DWORD PTR [rbx],0x1
# instead of
#     mov    eax,DWORD PTR [rbx]
#     sub    eax,0x1
#     mov    DWORD PTR [rbx],eax
# '-fstore-merging' is so `*func = (struct thunk){1, 0, {beta_25}};` codegens to
#     mov    QWORD PTR [rbp+0x0],0x1
#     mov    QWORD PTR [rbp+0x8],0x40179d
# instead of
#     mov    DWORD PTR [rbp+0x0],0x1
#     mov    DWORD PTR [rbp+0x4],0x0
#     mov    QWORD PTR [rbp+0x8],0x40179d

pnlc: pnlc.c; $(CC) $(CFLAGS) -o $@ pnlc.c -Wno-parentheses -Wno-sign-compare -Wno-unused-value -Wno-implicit-fallthrough -Wno-missing-field-initializers -Wno-self-assign
emit: emit.c; $(CC) $(CFLAGS) -o $@ emit.c $(EMIT) -Wno-parentheses -Wno-unused-parameter -Wno-misleading-indentation
self: self.c; $(CC) $(CFLAGS) -o $@ self.c $(EMIT) -Wno-parentheses -Wno-unused-parameter -Wno-misleading-indentation

# seed the bootstrap of the self-compiler 'pnlc definitional.pnlc' using the
# translator 'pnlc.c'. this can be done either by compiling the self-compiler
# directly, or by interpreting the self-compiler compiling itself.
seed: pnlc
	# ./pnlc prelude.pnlc --emit examples/pnlc\ definitional.pnlc && mv emit.c self.c
	cat -- prelude.pnlc --emit $(INJECT) examples/pnlc\ definitional.pnlc | ./pnlc prelude.pnlc examples/pnlc\ definitional.pnlc 2> self.c
	$(MAKE) self

# bootstrap the self-compiler 'pnlc definitional.pnlc'. when `diff` reports
# no differences, a fixed point has been attained. remember that when updating
# codegen, you need to bootstrap twice: once to obtain a compiler producing the
# new codegen, then again to obtain a compiler running on the new codegen. a
# third bootstrap should then report no differences.
bootstrap:
	cp self.c self.orig.c
	cat -- prelude.pnlc --emit $(INJECT) examples/pnlc\ definitional.pnlc | ./self 2> self.c
	$(MAKE) self
	diff -q -s self.c self.orig.c || true
