#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// keep in sync with 'examples/pnlc definitional.pnlc',
// 'examples/pnlc metacircular.pnlc' and 'examples/pnlc shallow.pnlc'

enum { TYPE_APP, TYPE_LAM, TYPE_VAR, TYPE_MVAR };

// "IO"s are opaque lambda-terms that are handled in special ways. when `term.
// type < 0`, the term is an IO, and `~term.type` will be one of the following:
// clang-format off
enum {IO_EXIT, IO_ERR, IO_GET, IO_PUT, IO_EPUT,
      IO_DUMP, IO_ONE, IO_ZERO, IO_LEN};
// clang-format on
char *ios[] = {"$exit", "$err", "$get",  "$put", "$eput",
               "$dump", "#one", "#zero", NULL};

// a `struct term` is a node in a directed acyclic graph. `refcount` is the
// in-degree. `beta` is a borrow, and together with `visited` it forms a cache
// for beta-reduction. for applications, when `type = TYPE_APP`, `lhs` is the
// function and `rhs` is the argument. for abstractions, when `type = TYPE_LAM`,
// `lhs` is the variable and `rhs` is the body. several abstraction nodes might
// bind the same variable node, in which case its `type = TYPE_MVAR`
struct term {
  int type;
  unsigned refcount;
  long long visited;
  struct term *lhs, *rhs;
  struct term *beta;
};

#define APP(LHS, RHS)                                                          \
  term_alloc((struct term){TYPE_APP, .lhs = LHS, .rhs = RHS})
#define LAM(LHS, RHS)                                                          \
  term_alloc((struct term){TYPE_LAM, .lhs = LHS, .rhs = RHS})
#define VAR() term_alloc((struct term){TYPE_VAR})
#define IO(TYP) term_alloc((struct term){~(TYP)})

// a variable node that also stores the text of its name and a "next" pointer to
// form an environment linked list
#define ENV(NAME_BEGIN, NAME_END, NEXT)                                        \
  term_alloc((struct term){TYPE_VAR, .lhs = (void *)(NAME_BEGIN),              \
                           .rhs = (void *)(NAME_END), .beta = NEXT})

struct term *term_alloc(struct term fields) {
  struct term *term = malloc(sizeof *term);
  fields.refcount = 1;
  return *term = fields, term;
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
      free(term->lhs); // fast path; always a `TYPE_VAR` or `TYPE_MVAR`
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
    return term->beta = term;

  if (term->visited == visited) {
    struct term *beta = term_incref(term->beta);
    return term_decref(term), beta;
  }

  switch (term->type) {
  case TYPE_APP: {
    if (term->refcount == 1) {
      term->lhs = beta(term->lhs, var, arg, visited);
      term->rhs = beta(term->rhs, var, arg, visited);
      term->beta = term;
      break;
    }
    struct term *lhs = beta(term_incref(term->lhs), var, arg, visited);
    struct term *rhs = beta(term_incref(term->rhs), var, arg, visited);
    if (lhs == term->lhs && rhs == term->rhs)
      term_decref(lhs), term_decref(rhs), term->beta = term;
    else
      term_decref(term), term->beta = APP(lhs, rhs);
  } break;
  case TYPE_LAM: {
    if (term->lhs == var ? term->beta = term : 0)
      break; // stop recursing, this abstraction shadows the top-level one
    if (term->refcount == 1) {
      term->rhs = beta(term->rhs, var, arg, visited);
      term->beta = term;
      break;
    }
    struct term *rhs = beta(term_incref(term->rhs), var, arg, visited);
    if (rhs == term->rhs)
      term_decref(rhs), term->beta = term;
    // we're binding a bound variable, so set it as multiply-bound
    else if (term->lhs->type = TYPE_MVAR)
      term_decref(term), term->beta = LAM(term_incref(term->lhs), rhs);
  } break;
  case TYPE_VAR:
  case TYPE_MVAR:
    term->beta = term == var ? term_decref(term), term_incref(arg) : term;
    break;
  default /* IO */:
    term->beta = term;
    break;
  }

  term->visited = visited;
  return term->beta; // move out
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
  return whnf(term, visited);
}

// bit stream
struct bs {
  FILE *fp;
  int n;
  int c;
};

bool bs_eof(struct bs *bs) { return bs->c == EOF; }

bool bs_get(struct bs *bs) {
  if (bs->n == 0)
    bs->n = CHAR_BIT, bs->c = fgetc(bs->fp);
  return bs->c >> CHAR_BIT - bs->n-- & 1;
}

void bs_put(struct bs *bs, bool bit) {
  bs->c |= bit << bs->n++;
  if (bs->n == CHAR_BIT)
    bs->n = 0, fputc(bs->c, bs->fp), bs->c = 0;
}

char *run(struct term **term, struct bs *bs_in, struct bs *bs_out,
          struct bs *bs_err, long long *visited) {
  // takes ownership of `*term`. upon successful termination, returns `NULL` and
  // writes `NULL` into `*term`; otherwise, returns an error message and stores
  // the problematic term into `*term`

  for (struct term *cont;; term_decref(*term), *term = cont) {
    struct term *head = whnf(*term, visited);
    switch (~head->type) {
    case IO_ERR:
      return "hit $err at top level";
    case IO_EXIT:
      if (~head->visited != 0)
        return "$exit expects 0 arguments";
      *term = term_decref(*term);
      return NULL;
    case IO_DUMP:
      if (~head->visited != 2)
        return "$dump expects 2 arguments";
      term_dump((*term)->lhs->rhs, ++*visited), fputc('\n', stderr);
      cont = term_incref((*term)->rhs);
      break;
    case IO_GET: {
      if (~head->visited != 1)
        return "$get expects 1 argument";
      fflush(bs_out->fp), fflush(bs_err->fp);
      bool bit = bs_get(bs_in), eof = bs_eof(bs_in);
      struct term *some, *none, *one, *zero;
      // clang-format off
      struct term *arg =
          (some = VAR(), LAM(some,
          (none = VAR(), LAM(none,
              eof ? term_incref(none)
                  : APP(term_incref(some),
                        (one = VAR(), LAM(one,
                        (zero = VAR(), LAM(zero,
                            term_incref(bit ? one : zero))))))))));
      // clang-format on
      cont = APP(term_incref((*term)->rhs), arg);
    } break;
    case IO_PUT:
    case IO_EPUT: {
      bool isput = ~head->type == IO_PUT;
      if (~head->visited != 2)
        return isput ? "$put expects 2 arguments" //
                     : "$eput expects 2 arguments";
      struct term *bit =
          APP(APP(term_incref((*term)->lhs->rhs), IO(IO_ONE)), IO(IO_ZERO));
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
    default:
      return "top level is irreducible";
    }
  }
}

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

    return APP(lhs, rhs);
  }
  case '\\': {
    parse_ws(prog);

    char *begin = *prog, *end = parse_var(prog, error);
    if (*error)
      return NULL;

    struct term *lhs = env = ENV(begin, end, env);
    struct term *rhs = parse_term(prog, error, env);
    if (*error)
      return term_decref(lhs), NULL;

    // uncomment this to avoid holding any pointers into `prog` when we return.
    // variables won't store the text of their names and `term_dump` will dump
    // them as their memory addresses instead.
    // lhs->lhs = lhs->rhs = NULL;

    return LAM(lhs, rhs);
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

    for (; env; env = env->beta)
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
    env = ENV(*io, *io + strlen(*io), env);

  struct term *term = parse_term(prog, error, env);
  for (int typ = IO_LEN; typ--; env = env->beta)
    term = APP(LAM(env, term ? term : IO(0)), IO(typ));
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

  free(buf);
}
