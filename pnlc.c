#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// keep in sync with 'examples/pnlc definitional.pnlc',
// 'examples/pnlc metacircular.pnlc' and 'examples/pnlc shallow.pnlc'

enum { TYPE_APP, TYPE_LAM, TYPE_VAR, TYPE_MVAR };

// when `term.type < 0`, the term stands for an I/O action, and `~term.type`
// will be one of the following:
// clang-format off
enum {IO_EXIT, IO_ERR,  IO_EMIT, IO_GET,  IO_PUT,
      IO_EPUT, IO_DUMP, IO_ONE,  IO_ZERO, IO_LEN};
// clang-format on
// `IO_ONE` and `IO_ZERO` are for internal use. we name them "#one" and "#zero"
// so user programs can't reference them (they would parse as comments)
char *ios[] = {"$exit", "$err",  "$emit", "$get",  "$put",
               "$eput", "$dump", "#one",  "#zero", NULL};

// a `struct term` is a node in a directed acyclic graph. `refcount` is the
// in-degree. `beta` is a borrow, and together with `visited` it forms a cache
// for beta-reduction. `env` is for storing free variables during codegen. for
// applications, when `type = TYPE_APP`, `lhs` is the function and `rhs` is the
// argument. for abstractions, when `type = TYPE_LAM`, `lhs` is the variable and
// `rhs` is the body. several abstraction nodes might bind the same variable
// node, in which case its `type = TYPE_MVAR`
struct term {
  int type;
  unsigned refcount;
  long long visited;
  struct term *lhs, *rhs;
  union {
    struct term *beta;
    size_t env;
  } u;
};

#define TERM(...) (&(struct term){__VA_ARGS__})

#define APP(LHS, RHS) TERM(TYPE_APP, .lhs = LHS, .rhs = RHS)
#define LAM(LHS, RHS) TERM(TYPE_LAM, .lhs = LHS, .rhs = RHS)
#define VAR() TERM(TYPE_VAR)
#define IO(TYP) TERM(~(TYP))

// a variable node that also stores the text of its name and a "next" pointer to
// form an environment linked list
#define ENV(NAME_BEGIN, NAME_END, NEXT)                                        \
  TERM(TYPE_VAR, .lhs = (void *)(NAME_BEGIN), .rhs = (void *)(NAME_END),       \
       .u.beta = NEXT)

struct term *term_alloc(struct term fields) {
  struct term *term = malloc(sizeof *term);
  fields.refcount = 1;
  // spell out `*term = fields` manually, otherwise GCC's scalar replacement
  // of aggregates (SRA) chokes on the union and the compound literals backing
  // `fields` get allocated on the stack, resulting in a 10-20% global slowdown
  term->type = fields.type;
  term->refcount = fields.refcount;
  term->visited = fields.visited;
  term->lhs = fields.lhs;
  term->rhs = fields.rhs;
  term->u.beta = fields.u.beta;
  return term;
}

struct term *term_dump(struct term *term, long long visited) {
  if (term->visited < 0 != visited < 0)
    visited = -visited; // preserve terms marked as closed

  // uncomment to dump already-dumped terms as single '@' characters. the dump
  // will be ambiguous but its length will be linear, not exponential, in the
  // amount of memory `term` uses
  // if (term->visited == visited)
  //   switch (term->type)
  //   case TYPE_APP:
  //   case TYPE_LAM:
  //     return fputs("@ ", stderr), term;

  // uncomment to dump refcounts. can make the dump harder to read
  // for (int i = 1; i < term->refcount; i++)
  //   fputc(term->visited == visited ? '<' : '>', stderr);

  // uncomment to dump whether terms are marked as closed
  // if (term->visited < 0)
  //   fputc('#', stderr);

  term->visited = visited;
  switch (term->type) {
  case TYPE_APP:
    fputc('.', stderr);
    term_dump(term->lhs, visited), term_dump(term->rhs, visited);
    break;
  case TYPE_LAM:
    fputc('\\', stderr);
    term_dump(term->lhs, visited), term_dump(term->rhs, visited);
    break;
  case TYPE_VAR:
  case TYPE_MVAR:
    if (term->lhs && term->rhs) {
      char *begin = (char *)term->lhs, *end = (char *)term->rhs;
      fprintf(stderr, "%.*s ", (int)(end - begin), begin);
    } else
      fprintf(stderr, "%p ", (void *)term);
    break;
  default /* IO */:
    fprintf(stderr, "%s ", ios[~term->type]);
    break;
  }
  return term;
}

struct term *term_incref(struct term *term) {
  return term->refcount++, term; //
}

struct term *term_decref(struct term *term) {
  // always returns `NULL` so you can go `term = term_decref(term);`

again:
  if (--term->refcount)
    return NULL;

  switch (term->type) {
  case TYPE_APP:
    term_decref(term->lhs);
    break;
  case TYPE_LAM:
    if (!--term->lhs->refcount)
      free(term->lhs); // fast path; always a variable
    break;
  case TYPE_VAR:
  case TYPE_MVAR:
  default /* IO */:
    return free(term), NULL;
  }

  // manual tail call optimization, otherwise GCC doesn't see it and we overflow
  // the stack when trying to free terms that are several gigabytes in size
  struct term *rhs = term->rhs;
  free(term), term = rhs;
  goto again;
}

// reduction to weak normal form or to weak head normal form only ever beta-
// reduces applications whose argument is closed, because the top-level term is
// always closed and neither algorithm recurses into abstractions. this means
// that naive substitution with no alpha-conversion is sufficient, and thus
// the only parts of the lambda body that need to be copied are the transitive
// parents of the variable being substituted. in particular, we can clone an
// abstraction node without cloning the variable node it binds, so the parts of
// the abstraction body that depend on that variable but not on the variable
// being substituted don't need to be copied either. as a result, several
// abstraction nodes might bind the same variable node, and the intuition is
// the usual one: when one of the abstractions contains another, the inner
// abstraction shadows the outer one

struct term *beta(struct term *term, struct term *var, struct term *arg,
                  long long visited) {
  // returns the result of substituting `var` for `arg` in `term`. moves in
  // `term` but borrows `var` and `arg`. subterms whose `visited` is set to a
  // negative value are not recursed into. we cache intermediate results in
  // `beta` fields to ensure the graph doesn't degenerate to a tree. `beta`
  // fields hold weak references, which is safe because this function only
  // ever calls `term_decref` on terms whose `refcount > 1`

  // it can be good to test user programs with these two lines commented out
  // because then space leaks become gradual performance degradation
  if (term->visited < 0)
    return term->u.beta = term;

  if (term->visited == visited) {
    struct term *beta = term_incref(term->u.beta);
    return term_decref(term), beta;
  }

  switch (term->type) {
  case TYPE_APP: {
    if (term->refcount == 1) {
      term->lhs = beta(term->lhs, var, arg, visited);
      term->rhs = beta(term->rhs, var, arg, visited);
      term->u.beta = term;
      break;
    }
    struct term *lhs = beta(term_incref(term->lhs), var, arg, visited);
    struct term *rhs = beta(term_incref(term->rhs), var, arg, visited);
    if (lhs == term->lhs && rhs == term->rhs)
      term_decref(lhs), term_decref(rhs), term->u.beta = term;
    else
      term_decref(term), term->u.beta = term_alloc(*APP(lhs, rhs));
  } break;
  case TYPE_LAM: {
    if (term->lhs == var ? term->u.beta = term : 0)
      break; // stop recursing, this abstraction shadows the top-level one
    if (term->refcount == 1) {
      term->rhs = beta(term->rhs, var, arg, visited);
      term->u.beta = term;
      break;
    }
    struct term *rhs = beta(term_incref(term->rhs), var, arg, visited);
    if (rhs == term->rhs)
      term_decref(rhs), term->u.beta = term;
    // we're binding a bound variable, so set it as multiply-bound
    else if (term->lhs->type = TYPE_MVAR)
      term_decref(term),
          term->u.beta = term_alloc(*LAM(term_incref(term->lhs), rhs));
  } break;
  case TYPE_VAR:
  case TYPE_MVAR:
    term->u.beta = term == var ? term_decref(term), term_incref(arg) : term;
    break;
  default /* IO */:
    term->u.beta = term;
    break;
  }

  term->visited = visited;
  return term->u.beta; // move out
}

// beta-reduction searches the body of an abstraction for occurrences of the
// variable it binds, so the larger the functions being called, the slower
// things get. this is an important problem because Scott-encoded data is
// functions and projections on the data is calling those functions, so the
// larger the data structures in a user program the slower it runs, regardless
// of how much of that data it processes. the solution is as follows. reduction
// to weak normal form or to weak head normal form only ever beta-reduces
// applications whose argument is closed, because the top-level term is always
// closed and neither algorithm recurses into abstractions. so before calling
// into beta-reduction we can mark the argument as closed. then, we amend beta-
// reduction to skip searching terms marked as closed, because no substitutions
// would take place there anyway. since variables are only ever substituted for
// marked terms, beta-reduction doesn't slow down as functions increase in size

struct term *whnf(struct term *term, long long *visited) {
  // reduce to weak head normal form using normal-order semantics. this means we
  // reduce the leftmost outermost redex first and ignore any redexes inside
  // abstractions or in the argument position of applications. the resulting
  // beta-reduction of `term` is written into `*term` itself so the computation
  // is shared across pointees. returns a borrow to the head term and stores the
  // bitwise complement of its depth in its `visited` field

again:
  if (term->type != TYPE_APP)
    return term->visited = ~0, term; // head, closed

  // the head term and its arguments are always closed. set their `visited` to a
  // negative value so `beta` skips searching them
  term->rhs->visited = -1; // closed

  struct term *head = whnf(term->lhs, visited);
  if (term->lhs->type != TYPE_LAM)
    return head->visited--, head; // increment depth

  // we do some gymnastics to make sure `term` doesn't hold a reference to
  // `body` because `beta` can avoid an allocation when its `refcount` is 1.
  struct term *var = term_incref(term->lhs->lhs),
              *body = term_incref(term->lhs->rhs),
              *arg = term_incref(term->rhs);
  unsigned lam_refcount = term->lhs->refcount;
  term_decref(term->lhs), term_decref(term->rhs); // move out
  // small optimization: if `term` held the only reference to the abstraction
  // node and the abstraction node was the only binder of `var`, we can just
  // memcpy `*arg` into `*var` and skip calling `beta`. we only do so when we
  // hold the only reference to `arg`, else we might induce duplicate work.
  // this speeds up the evaluation of top-level definitions, and the prelude
  // is nothing but top-level definitions, so it also noticeably improves the
  // start-up time of user programs
  if (lam_refcount == 1 && var->type != TYPE_MVAR && arg->refcount == 1) {
    // uncomment and you should see all prelude definitions
    // term_dump(var, ++*visited);
    // no need to ever set `arg->lhs` as a multiply-bound variable because
    // `arg->refcount == 1` and we're about to `term_decref(arg)`
    var->lhs = arg->lhs ? term_incref(arg->lhs) : NULL;
    var->rhs = arg->rhs ? term_incref(arg->rhs) : NULL;
    var->type = arg->type, var->visited = arg->visited;
  } else
    body = beta(body, var, arg, ++*visited);
  term_decref(var), term_decref(arg);
  // only set `var` as a multiply-bound variable when `body->refcount > 1`
  // because we're about to `term_decref(body)`
  if (body->type == TYPE_LAM && body->refcount > 1)
    body->lhs->type = TYPE_MVAR;
  term->lhs = body->lhs ? term_incref(body->lhs) : NULL;
  term->rhs = body->rhs ? term_incref(body->rhs) : NULL;
  term->type = body->type, term->visited = body->visited;
  term_decref(body);
  goto again;
}

// allow stringizing without recursive macro expansion
#define STRINGIZABLE(...) (#__VA_ARGS__, __VA_ARGS__)
#define STRINGIZED_(STRINGIZED, ...) STRINGIZED
#define EXPANDED_(STRINGIZED, ...) __VA_ARGS__
#define STRINGIZED(STRINGIZABLE) STRINGIZED_ STRINGIZABLE
#define EXPANDED(STRINGIZABLE) EXPANDED_ STRINGIZABLE

#define BITSTREAM_LIB                                                          \
  /* when stringizing we don't want macros like `CHAR_BIT` expanding */        \
  STRINGIZABLE(                                                                \
      struct bs {                                                              \
        FILE *fp;                                                              \
        int n;                                                                 \
        int c;                                                                 \
      };                                                                       \
                                                                               \
      bool bs_eof(struct bs *bs) { return bs->c == EOF; }                      \
                                                                               \
      bool bs_get(struct bs *bs) {                                             \
        if (bs->n == 0)                                                        \
          bs->n = CHAR_BIT, bs->c = fgetc(bs->fp);                             \
        return bs->c >> CHAR_BIT - bs->n-- & 1;                                \
      }                                                                        \
                                                                               \
      void bs_put(struct bs *bs, bool bit) {                                   \
        bs->c |= bit << bs->n++;                                               \
        if (bs->n == CHAR_BIT)                                                 \
          bs->n = 0, fputc(bs->c, bs->fp), bs->c = 0;                          \
      })

EXPANDED(BITSTREAM_LIB)

char *run(struct term **term, struct bs *bs_in, struct bs *bs_out,
          struct bs *bs_err, long long *visited) {
  // takes ownership of `*term`. on error, stores the problematic term into
  // `*term` and returns an error message. on `$emit`, writes the continuation
  // to `*term` and returns `NULL`. on `$exit`, writes `NULL` to `*term` and
  // returns `NULL`

  for (struct term *cont;; term_decref(*term), *term = cont) {
    struct term *head = whnf(*term, visited);
    switch (~head->type) {
    case IO_EXIT:
      if (~head->visited != 0)
        return "$exit expects 0 arguments";
      *term = term_decref(*term);
      return NULL;
    case IO_ERR:
      return "hit $err at top level";
    case IO_EMIT:
      if (~head->visited != 1)
        return "$emit expects 1 argument";
      cont = term_incref((*term)->rhs);
      term_decref(*term), *term = cont;
      return NULL;
    case IO_GET: {
      if (~head->visited != 1)
        return "$get expects 1 argument";
      fflush(bs_out->fp), fflush(bs_err->fp);
      bool bit = bs_get(bs_in), eof = bs_eof(bs_in);
      struct term *some, *none, *one, *zero;
      // clang-format off
      struct term *arg =
          (some = term_alloc(*VAR()), term_alloc(*LAM(some,
          (none = term_alloc(*VAR()), term_alloc(*LAM(none,
              eof ? term_incref(none) : term_alloc(*APP(term_incref(some),
                  (one  = term_alloc(*VAR()), term_alloc(*LAM(one,
                  (zero = term_alloc(*VAR()), term_alloc(*LAM(zero,
                      term_incref(bit ? one : zero)))))))))))))));
      // clang-format on
      cont = term_alloc(*APP(term_incref((*term)->rhs), arg));
    } break;
    case IO_PUT:
    case IO_EPUT: {
      bool isput = ~head->type == IO_PUT;
      if (~head->visited != 2)
        return isput ? "$put expects 2 arguments" //
                     : "$eput expects 2 arguments";
      // clang-format off
      struct term *bit =
          term_alloc(*APP(term_alloc(*APP(term_incref((*term)->lhs->rhs),
          term_alloc(*IO(IO_ONE)))), term_alloc(*IO(IO_ZERO))));
      // clang-format on
      if (~whnf(bit, visited)->type == IO_ERR)
        return term_decref(bit), isput ? "hit $err in $put argument"
                                       : "hit $err in $eput argument";
      if (~bit->type != IO_ONE && ~bit->type != IO_ZERO)
        return term_decref(bit), isput ? "$put argument is malformed"
                                       : "$eput argument is malformed";
      bs_put(isput ? bs_out : bs_err, ~bit->type == IO_ONE), term_decref(bit);
      // uncomment to disable buffering of user program output
      // fflush(bs_out->fp), fflush(bs_err->fp);
      cont = term_incref((*term)->rhs);
    } break;
    case IO_DUMP:
      if (~head->visited != 2)
        return "$dump expects 2 arguments";
      term_dump((*term)->lhs->rhs, ++*visited), fputc('\n', stderr);
      cont = term_incref((*term)->rhs);
      break;
    default:
      return "top level is irreducible";
    }
  }
}

// every term is compiled to a function, a "beta_" function, that knows how to
// beta-reduce an argument into that term. the exception is variables, which are
// inlined. terms at runtime become thunks, which consist of the term's "beta_"
// function plus an environment of further thunks. evaluation of the top-level
// thunk is kick-started by beta-reducing in the sentinel value `&thread`.
// environments own their thunks. "beta_" functions move in their parameters
// and return owned thunks.

#define P(...) fprintf(fp, __VA_ARGS__)

struct env {
  struct term *var;
  unsigned len;
  size_t next;
};

void emit(FILE *fp, struct term *term, struct env **arena, size_t *top,
          size_t env, long long visited) {
  // codegens `term` into `fp` and returns its set of free variables in its
  // `env` field. `env` the parameter is the set of variables bound by parent
  // abstraction nodes. both are allocated in `*arena`, by bumping `*top` and
  // `realloc`ing when needed. we use `visited` to ensure the graph doesn't
  // degenerate to a tree. the values of `env` fields are kept unique so they
  // can double as UIDs for naming "beta_" functions

  if (term->visited == visited)
    return;

  // macros to allocate new environment nodes in the arena. index 0 of `*arena`
  // is already a nil, so only use `NIL` when you really need a brand new node
  size_t tail;
#define NIL                                                                    \
  ((++*top, *top & *top - 1) ||                                                \
       (*arena = realloc(*arena, sizeof **arena * *top * 2)),                  \
   (*top)[*arena] = (struct env){NULL, 0, 0}, *top)
#define CONS(HEAD, TAIL)                                                       \
  (tail = TAIL, NIL,                                                           \
   (*top)[*arena] = (struct env){HEAD, tail[*arena].len + 1, tail},            \
   tail = tail[*arena].next, *top)

  switch (term->type) {
  case TYPE_APP: {
    term->u.env = NIL;
    emit(fp, term->lhs, arena, top, env, visited);
    emit(fp, term->rhs, arena, top, env, visited);
    size_t lhs_env = term->lhs->u.env;
    size_t rhs_env = term->rhs->u.env;

    P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {",
      term->u.env);
    if (term->lhs->type < TYPE_VAR)
      P("  struct thunk *func = malloc(sizeof *func + sizeof arg * %u);"
        "  *func = (struct thunk){1, %u, beta_%zu};",
        lhs_env[*arena].len ? lhs_env[*arena].len : term->lhs->type == TYPE_APP,
        lhs_env[*arena].len, lhs_env);
    else
      P("  struct thunk *func;");
    if (term->rhs->type < TYPE_VAR)
      P("  struct thunk *arg_ = malloc(sizeof *arg_ + sizeof arg * %u);"
        "  *arg_ = (struct thunk){1, %u, beta_%zu};",
        rhs_env[*arena].len ? rhs_env[*arena].len : term->rhs->type == TYPE_APP,
        rhs_env[*arena].len, rhs_env);
    else
      P("  struct thunk *arg_;");

    size_t rev_env = 0;
    unsigned term_idx = 0, lhs_idx = 0, rhs_idx = 0;
    for (; env[*arena].var; env = env[*arena].next) {
      if (lhs_env[*arena].var == env[*arena].var) {
        if (term->lhs->type < TYPE_VAR)
          P("  func->env[%u] = self->env[%u];", lhs_idx, term_idx);
        else
          P("  func = self->env[%u];", term_idx);
        if (rhs_env[*arena].var == env[*arena].var)
          P("  self->env[%u]->refcount++;", term_idx);
        else
          rev_env = CONS(env[*arena].var, rev_env), term_idx++;
        lhs_env = lhs_env[*arena].next, lhs_idx++;
      }
      if (rhs_env[*arena].var == env[*arena].var) {
        if (term->rhs->type < TYPE_VAR)
          P("  arg_->env[%u] = self->env[%u];", rhs_idx, term_idx);
        else
          P("  arg_ = self->env[%u];", term_idx);
        rev_env = CONS(env[*arena].var, rev_env), term_idx++;
        rhs_env = rhs_env[*arena].next, rhs_idx++;
      }
    }
    size_t term_env = 0;
    while (rev_env[*arena].var) {
      rev_env[*arena].len = term_env[*arena].len + 1;
      size_t next = rev_env[*arena].next;
      rev_env[*arena].next = term_env, term_env = rev_env, rev_env = next;
    }
    term->u.env[*arena] = term_env[*arena];

    P("  func = func->beta(func, arg_);"
      // memoize the result of applying `func` to `arg_` into `*self->env`,
      // and set our "beta_" function to `beta_0` so future reductions use that
      // memoized value. for this to work, all thunks standing for application
      // nodes need to carry at least one environment slot, so application nodes
      // with no free variables have to be handled with care
      "  if (--self->refcount) {"
      "    self->envlen = 1, self->beta = beta_0;"
      "    (*self->env = func)->refcount++;"
      "  } else"
      "    free(self);"
      "  return func->beta(func, arg);"
      "}\n");
  } break;

  case TYPE_LAM: {
    term->u.env = NIL;
    env = CONS(term->lhs, env);
    emit(fp, term->rhs, arena, top, env, visited);
    size_t rhs_env = term->rhs->u.env;

    P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {",
      term->u.env);
    if (term->rhs->type < TYPE_VAR)
      P("  struct thunk *body = malloc(sizeof *body + sizeof arg * %u);"
        "  *body = (struct thunk){1, %u, beta_%zu};",
        rhs_env[*arena].len ? rhs_env[*arena].len : term->rhs->type == TYPE_APP,
        rhs_env[*arena].len, rhs_env);
    else
      P("  struct thunk *body;");

    unsigned term_idx = 0, rhs_idx = 0;
    if (rhs_env[*arena].var == env[*arena].var) {
      if (term->rhs->type < TYPE_VAR)
        P("  body->env[%u] = arg;", rhs_idx);
      else
        P("  body = arg;");
      rhs_env = rhs_env[*arena].next, rhs_idx++;
    } else
      P("  decref(arg);");
    for (; term_idx < rhs_env[*arena].len; rhs_idx++, term_idx++) {
      if (term->rhs->type < TYPE_VAR)
        P("  (body->env[%u] = self->env[%u])->refcount++;", rhs_idx, term_idx);
      else
        P("  (body = self->env[%u])->refcount++;", term_idx);
    }
    term->u.env[*arena] = rhs_env[*arena];

    P("  return decref(self), body;"
      "}\n");
  } break;

  case TYPE_VAR:
  case TYPE_MVAR:
    term->u.env = CONS(term, 0);
    break;

// the idea for I/O actions is to accumulate arguments into the environment
// until enough have been received, at which point we make sure the latest
// argument is the `&thread` sentinel sent from `main`, and we perform the
// action. this `OPAQUE(ENV)` macro builds terms that, when sent to `codegen`,
// emit nothing but pretend to have free variables `ENV`.
#define OPAQUE(ENV) TERM(.visited = visited, .u.env = ENV)

  default /* IO */:
    switch (~term->type) {
    case IO_EXIT: {
      struct term *exit = OPAQUE(NIL);
      P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {"
        "  if (arg != &thread) abort();"
        "  return decref(self), &thread;"
        "}\n",
        exit->u.env);
      emit(fp, exit = exit, arena, top, 0, visited);
      term->u.env = exit->u.env;
    } break;

    case IO_ERR: {
      struct term *err = OPAQUE(NIL);
      P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {"
        "  if (arg != &thread) abort();"
        "  decref(self), abort();"
        "}\n",
        err->u.env);
      emit(fp, err = err, arena, top, 0, visited);
      term->u.env = err->u.env;
    } break;

    case IO_EMIT: {
      // in compiled code, `$emit` becomes a no-op.
      struct term *cont = VAR();
      struct term *emit_ = OPAQUE(CONS(cont, 0));
      P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {"
        "  if (arg != &thread) abort();"
        "  struct thunk *cont = self->env[0];"
        "  cont->refcount++, decref(self);"
        "  return cont->beta(cont, &thread);"
        "}\n",
        emit_->u.env);
      emit(fp, emit_ = LAM(cont, emit_), arena, top, 0, visited);
      term->u.env = emit_->u.env;
    } break;

    case IO_GET: {
      struct term *some = VAR(), *none = VAR(), *one = VAR(), *zero = VAR(),
                  *cont = VAR();
      struct term *bit_eof = LAM(some, LAM(none, none));
      struct term *bit_one =
          LAM(some, LAM(none, APP(some, LAM(one, LAM(zero, one)))));
      struct term *bit_zero =
          LAM(some, LAM(none, APP(some, LAM(one, LAM(zero, zero)))));
      emit(fp, bit_eof, arena, top, 0, visited);
      emit(fp, bit_one, arena, top, 0, visited);
      emit(fp, bit_zero, arena, top, 0, visited);
      struct term *get = OPAQUE(CONS(cont, 0));
      P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {"
        "  if (arg != &thread) abort();"
        "  struct thunk *cont = self->env[0];"
        "  cont->refcount++, decref(self);"
        "  fflush(bs_out.fp), fflush(bs_err.fp);"
        "  bool bit = bs_get(&bs_in), iseof = bs_eof(&bs_in);"
        "  static struct thunk eof = {INT_MAX, 0, beta_%zu};"
        "  static struct thunk one = {INT_MAX, 0, beta_%zu};"
        "  static struct thunk zero = {INT_MAX, 0, beta_%zu};"
        "  cont = cont->beta(cont, iseof ? &eof : bit ? &one : &zero);"
        "  return cont->beta(cont, &thread);"
        "}\n",
        get->u.env, bit_eof->u.env, bit_one->u.env, bit_zero->u.env);
      emit(fp, get = LAM(cont, get), arena, top, 0, visited);
      term->u.env = get->u.env;
    } break;

    case IO_PUT:
    case IO_EPUT: {
      bool isput = ~term->type == IO_PUT;
      struct term *one = OPAQUE(NIL), *zero = OPAQUE(NIL);
      P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {"
        "  if (arg != &thread) abort();"
        "  return self;"
        "}\n",
        one->u.env);
      P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {"
        "  if (arg != &thread) abort();"
        "  return self;"
        "}\n",
        zero->u.env);
      struct term *bit = VAR(), *cont = VAR();
      struct term *put = OPAQUE(CONS(cont, CONS(bit, 0)));
      P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {"
        "  if (arg != &thread) abort();"
        "  struct thunk *cont = self->env[0];"
        "  struct thunk *bit = self->env[1];"
        "  cont->refcount++, bit->refcount++, decref(self);"
        "  static struct thunk one = {INT_MAX, 0, beta_%zu};"
        "  static struct thunk zero = {INT_MAX, 0, beta_%zu};"
        "  bit = bit->beta(bit, &one);"
        "  bit = bit->beta(bit, &zero);"
        "  bit = bit->beta(bit, &thread);"
        "  if (bit != &one && bit != &zero) abort();"
        "  bs_put(&%s, bit == &one);"
        "  return cont->beta(cont, &thread);"
        "}\n",
        put->u.env, one->u.env, zero->u.env, isput ? "bs_out" : "bs_err");
      emit(fp, put = LAM(bit, LAM(cont, put)), arena, top, 0, visited);
      term->u.env = put->u.env;
    } break;

    case IO_DUMP: {
      struct term *term_ = VAR(), *cont = VAR();
      struct term *dump = OPAQUE(CONS(cont, CONS(term_, 0)));
      P("struct thunk *beta_%zu(struct thunk *self, struct thunk *arg) {"
        "  if (arg != &thread) abort();"
        "  struct thunk *cont = self->env[0];"
        "  struct thunk *term = self->env[1];"
        "  dump(term), fputc('\\n', stderr);"
        "  cont->refcount++, decref(self);"
        "  return cont->beta(cont, &thread);"
        "}\n",
        dump->u.env);
      emit(fp, dump = LAM(term_, LAM(cont, dump)), arena, top, 0, visited);
      term->u.env = dump->u.env;
    } break;
    }

#undef OPAQUE
  }

  term->visited = visited;
#undef CONS
#undef NIL
}

void codegen(FILE *fp, struct term *term, long long visited) {
  P("#include <limits.h>\n"
    "#include <stdbool.h>\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    ""
    "struct thunk {"
    "  unsigned refcount, envlen;"
    "  struct thunk *(*beta)(struct thunk *self, struct thunk *arg);"
    "  struct thunk *env[];"
    "};"
    ""
    "void decref(struct thunk *thunk) {"
    "decref:"
    "  if (--thunk->refcount)"
    "    return;"
    "  for (unsigned e = 1; e < thunk->envlen; e++)"
    "    decref(thunk->env[e]);"
    "  if (thunk->envlen) {"
    "    struct thunk *env = *thunk->env;"
    "    free(thunk), thunk = env;"
    "    goto decref;"
    "  }"
    "  free(thunk);"
    "}"
    ""
    "void dump(struct thunk *thunk) {"
    // we can't `printf("%p", thunk->beta);` because %p expects a `void *`
    // and function pointers can't be cast to `void *`. see ISO/IEC 9899:TC3,
    // $6.3 'Conversions' and $J.5.7 'Function pointer casts'
    "  union {"
    "    struct thunk *(*beta)(struct thunk *thunk, struct thunk *arg);"
    "    unsigned char buf[sizeof thunk->beta];"
    "  } u = {thunk->beta};"
    "  for (unsigned b = 0; b < sizeof u.buf; b++)"
    "    fprintf(stderr, \"%%02x\", u.buf[b]);"
    "  fputc('(', stderr);"
    "  for (unsigned e = 0; e < thunk->envlen; e++)"
    "    dump(thunk->env[e]);"
    "  fputc(')', stderr);"
    "}\n");

  P(STRINGIZED(BITSTREAM_LIB) "\n");

  P("struct bs bs_in, bs_out, bs_err;\n"
    ""
    "struct thunk thread = {INT_MAX, 0, NULL};"
    ""
    "struct thunk *beta_0(struct thunk *self, struct thunk *arg) {"
    "  struct thunk *memo = *self->env;"
    "  memo->refcount++, decref(self);"
    "  return memo->beta(memo, arg);"
    "}\n");

    struct env *arena = malloc(sizeof *arena);
    *arena = (struct env){NULL, 0, 0}; // NIL
    emit(fp, term, &arena, &(size_t){0}, 0, visited);
    free(arena);

    P("int main(void) {"
      "  bs_in.fp = stdin, bs_out.fp = stdout, bs_err.fp = stderr;"
      "  struct thunk *top = malloc(sizeof *top + sizeof top);"
      "  *top = (struct thunk){1, 0, NULL};"
      "  if (beta_%zu(top, &thread) != &thread) abort();"
      "}\n",
      term->u.env);
}

#undef P

// keep in sync with pnlc.vim, grammar.bnf, 'examples/pnlc definitional.pnlc',
// 'examples/pnlc metacircular.pnlc' and 'examples/pnlc shallow.pnlc'

void parse_ws(char **prog) {
  while (isspace(**prog))
    ++*prog;
}

char *parse_var(char **prog, char **error) {
  if (!**prog || isspace(**prog)) {
    *error = "expected variable";
    return NULL;
  }

  // be maximally permissive with identifier characters
  while (**prog && !isspace(**prog))
    ++*prog;

  char *end = *prog;
  parse_ws(prog);
  return end;
}

struct term *parse_term(char **prog, char **error, struct term *env) {
  if (!**prog) {
    *error = "expected term";
    return NULL;
  }

  switch (*(*prog)++) {
  case '.': {
    char *app = *prog - 1;
    parse_ws(prog);

    struct term *lhs = parse_term(prog, error, env);
    if (*error)
      return NULL;

    if (!**prog) {
      *error = "application without an argument", *prog = app;
      return term_decref(lhs), NULL;
    }

    struct term *rhs = parse_term(prog, error, env);
    if (*error)
      return term_decref(lhs), NULL;

    return term_alloc(*APP(lhs, rhs));
  }
  case '\\': {
    parse_ws(prog);

    char *begin = *prog, *end = parse_var(prog, error);
    if (*error)
      return NULL;

    struct term *lhs = env = term_alloc(*ENV(begin, end, env));
    struct term *rhs = parse_term(prog, error, env);
    if (*error)
      return term_decref(lhs), NULL;

    // uncomment this to avoid holding any pointers into `prog` when we return.
    // variables won't store the text of their names and `term_dump` will dump
    // them as their memory addresses instead.
    // lhs->lhs = lhs->rhs = NULL;

    return term_alloc(*LAM(lhs, rhs));
  }
  case '#': {
    char *nl = strchr(*prog, '\n');
    if (nl == NULL) {
      *error = "unterminated comment", --*prog;
      return NULL;
    }

    *prog = nl + 1, parse_ws(prog);
    return parse_term(prog, error, env);
  }
  default: {
    --*prog;

    char *begin = *prog, *end = parse_var(prog, error);
    if (*error)
      return NULL;

    for (; env; env = env->u.beta)
      if (end - begin == (char *)env->rhs - (char *)env->lhs &&
          strncmp(begin, (char *)env->lhs, end - begin) == 0)
        return term_incref(env);

    *error = "unbound variable", *prog = begin;
    return NULL;
  }
  }
}

struct term *parse(char **prog, char **error) {
  parse_ws(prog);

  struct term *env = NULL;
  for (char **io = ios; *io; io++)
    env = term_alloc(*ENV(*io, *io + strlen(*io), env));

  struct term *term = parse_term(prog, error, env);
  term = term ? term : term_alloc(*VAR()); // dummy
  for (int typ = IO_LEN; typ--; env = env->u.beta)
    term = term_alloc(*APP(term_alloc(*LAM(env, term)), term_alloc(*IO(typ))));
  if (*error)
    return term_decref(term), NULL;

  if (**prog) {
    *error = "trailing characters";
    return term_decref(term), NULL;
  }

  return term;
}

int main(int argc, char **argv) {
  if (argc <= 1)
    fputs("usage: pnlc <files...>\n", stderr), exit(EXIT_FAILURE);

  long sizes[argc];
  char *buf = NULL;
  size_t len = 0;

  long *size = sizes;
  for (char **file = argv + 1; *file; file++, size++) {
    FILE *fp = fopen(*file, "r");
    if (fp == NULL)
      perror("fopen"), exit(EXIT_FAILURE);
    if (fseek(fp, 0, SEEK_END) == -1)
      perror("fseek"), exit(EXIT_FAILURE);
    if ((*size = ftell(fp)) == -1)
      perror("ftell"), exit(EXIT_FAILURE);
    rewind(fp);

    buf = realloc(buf, len + *size);
    if (fread(buf + len, 1, *size, fp) != *size)
      perror("fread"), exit(EXIT_FAILURE);
    if (fclose(fp) == EOF)
      perror("fclose"), exit(EXIT_FAILURE);
    len += *size;
  }

  // a dummy unnamed file of size 1 containing the terminating null byte
  *size = 1;
  buf = realloc(buf, len + *size);
  buf[len] = '\0';

  char *error = NULL, *loc = buf;
  struct term *term = parse(&loc, &error);

  if (error) {
    char **file = argv + 1;
    size_t off = loc - buf;
    for (long *size = sizes; off >= *size; size++, file++)
      off -= *size;

    fprintf(stderr,
            *file ? "%s%s at %s[%zu] near '%.16s'\n" : "%s%s at end of input\n",
            "parse error: ", error, *file, off, loc);
    free(buf), exit(EXIT_FAILURE);
  }

  long long visited = 0;
  struct bs bs_in = {stdin}, bs_out = {stdout}, bs_err = {stderr};
  if (error = run(&term, &bs_in, &bs_out, &bs_err, &visited)) {
    // uncomment this to dump the top-level term on error
    // term_dump(term, ++visited), fputc('\n', stderr);

    fprintf(stderr, "runtime error: %s\n", error);
    free(buf), term_decref(term), exit(EXIT_FAILURE);
  }

  if (term) {
    FILE *fp = fopen("emit.c", "w");
    if (fp == NULL)
      perror("fopen"), free(buf), exit(EXIT_FAILURE);
    codegen(fp, term, ++visited), term_decref(term);
    if (fclose(fp) == EOF)
      perror("fclose"), free(buf), exit(EXIT_FAILURE);
  }

  free(buf);
}
