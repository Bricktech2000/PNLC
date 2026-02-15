#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// keep in sync with examples/pnlc.pnlc

enum { TYPE_APP, TYPE_LAM, TYPE_VAR };

// "IO"s are opaque lambda-terms that are handled in special ways. when `term.
// type < 0`, the term is an IO, and `~term.type` will be one of the following:
enum { IO_EXIT, IO_ERR, IO_GET, IO_PUT, IO_EPUT, IO_DUMP };
// keep in sync with examples/pnlc.pnlc, README.md, pnlc.vim and io\ hook.pnlc
char *ios[] = {"$exit", "$err", "$get", "$put", "$eput", "$dump", NULL};

// a `struct term` is a node in a directed acyclic graph. `refcount` is the
// in-degree. `beta` is a borrow, and together with `visited` it forms a cache
// for beta-reduction. for applications, when `type = TYPE_APP`, `lhs` is the
// function and `rhs` is the argument. for abstractions, when `type = TYPE_LAM`,
// `lhs` is the variable and `rhs` is the body. several abstraction nodes might
// bind the same variable node
struct term {
  int type;
  unsigned refcount;
  long long visited;
  struct term *lhs, *rhs;
  struct term *beta;
};

// keep track of the nubmer of abstraction nodes binding each variable node.
// whenever we allocate or free an abstraction node, we call BIND() or UNBIND()
// on the variable node it binds. of course a variable node's in-degree can be
// greater than 0x100, so NBINDS() is really an upper bound on the true number
// of binders and NREFS() is really a lower bound on the true in-degree
#define BIND(VAR) ((VAR)->refcount += 0x100, VAR)
#define UNBIND(VAR) ((VAR)->refcount -= 0x100, VAR)
#define NBINDS(VAR) ((VAR)->refcount / 0x100) // upper bound
#define NREFS(VAR) ((VAR)->refcount % 0x100)  // lower bound

#define APP(LHS, RHS)                                                          \
  term_alloc((struct term){TYPE_APP, .lhs = LHS, .rhs = RHS})
#define LAM(LHS, RHS)                                                          \
  term_alloc((struct term){TYPE_LAM, .lhs = LHS, .rhs = RHS})
#define VAR() term_alloc((struct term){TYPE_VAR})
#define IO(TYP) term_alloc((struct term){~(TYP)})

struct term *term_alloc(struct term fields) {
  struct term *term = malloc(sizeof(*term));
  fields.type == TYPE_LAM ? BIND(fields.lhs) : 0;
  fields.refcount = 1;
  return *term = fields, term;
}

struct term *term_dump(struct term *term, long long visited) {
  // uncomment to dump already-dumped terms as single '#' characters. the dump
  // will be ambiguous but its length will be linear, not exponential, in the
  // amount of memory `term` uses
  // if (term->visited == visited)
  //   switch (term->type)
  //   case TYPE_APP:
  //   case TYPE_LAM:
  //     return fputs("# ", stderr), term;

  // uncomment to dump refcounts. can make the dump harder to read
  // for (int i = 1; i < NREFS(term); i++)
  //   fputc(term->visited == visited ? '<' : '>', stderr);

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
    if (term->lhs && term->rhs) {
      char *begin = (char *)term->lhs, *end = (char *)term->rhs;
      fprintf(stderr, "%.*s ", (int)(end - begin), begin);
    } else
      fprintf(stderr, "%p ", (void *)term);
    break;
  default: // IO
    fprintf(stderr, "%s ", ios[~term->type]);
    break;
  }
  return term;
}

struct term *term_incref(struct term *term) { return term->refcount++, term; }
struct term *term_decref(struct term *term) {
  // always returns `NULL` so you can go `term = term_decref(term);`
  if (--term->refcount)
    return NULL;
  switch (term->type) {
  case TYPE_LAM:
    UNBIND(term->lhs);
  case TYPE_APP:
    term_decref(term->lhs), term_decref(term->rhs);
  case TYPE_VAR:
  default:; // IO
  }
  return free(term), NULL;
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
  // `term` but borrows `var` and `arg`. we cache intermediate results in
  // `beta` fields to ensure the graph doesn't degenerate to a tree. `beta`
  // fields hold weak references, which is safe because this function only
  // ever calls `term_decref` on terms whose `refcount > 1`

  if (term->visited == visited) {
    struct term *beta = term_incref(term->beta);
    return term_decref(term), beta;
  }

  switch (term->type) {
  case TYPE_APP: {
    struct term *lhs = beta(term_incref(term->lhs), var, arg, visited);
    struct term *rhs = beta(term_incref(term->rhs), var, arg, visited);
    if (lhs == term->lhs && rhs == term->rhs) {
      term_decref(lhs), term_decref(rhs), term->beta = term;
    } else if (term->refcount == 1) {
      term_decref(term->lhs), term->lhs = lhs;
      term_decref(term->rhs), term->rhs = rhs;
      term->beta = term;
    } else
      term_decref(term), term->beta = APP(lhs, rhs);
  } break;
  case TYPE_LAM: {
    if (term->lhs == var ? term->beta = term : 0)
      break; // stop recursing, this abstraction shadows the top-level one
    struct term *rhs = beta(term_incref(term->rhs), var, arg, visited);
    if (rhs == term->rhs)
      term_decref(rhs), term->beta = term;
    else if (term->refcount == 1) {
      term_decref(term->rhs), term->rhs = rhs;
      term->beta = term;
    } else {
      struct term *lhs = term_incref(term->lhs);
      term_decref(term), term->beta = LAM(lhs, rhs);
    }
  } break;
  case TYPE_VAR:
    term->beta = term == var ? term_decref(term), term_incref(arg) : term;
    break;
  default: // IO
    term->beta = term;
    break;
  }

  term->visited = visited;
  return term->beta; // move out
}

struct term *whnf(struct term *term, long long *visited) {
  // reduce to weak head normal form using normal-order semantics. this means we
  // reduce the leftmost outermost redex first and ignore any redexes inside
  // abstractions or in the argument position of applications. the resulting
  // beta-reduction of `term` is written into `*term` itself so the computation
  // is shared across pointees. returns a borrow to the head term and stores the
  // negation of its depth in its `visited` field

  if (term->type != TYPE_APP)
    return term->visited = 0, term;

  struct term *head = whnf(term->lhs, visited);
  if (term->lhs->type != TYPE_LAM)
    return head->visited--, head;

  // we do some gymnastics to make sure `term` doesn't hold a reference to
  // `body` because `beta` can avoid an allocation when its `refcount` is 1.
  struct term *var = term_incref(term->lhs->lhs),
              *body = term_incref(term->lhs->rhs),
              *arg = term_incref(term->rhs);
  term_decref(term->lhs), term_decref(term->rhs); // move out
  // small optimization: if `term` held the only reference to the abstraction
  // node and the abstraction node was the only binder of `var`, we can just
  // memcpy `*arg` into `*var` and skip calling `beta`. we only do so when we
  // hold the only reference to `arg`, else we might induce duplicate work
  if (NBINDS(var) == 0 && arg->refcount == 1) {
    (var->type = arg->type) == TYPE_LAM ? BIND(arg->lhs) : 0;
    var->lhs = arg->lhs ? term_incref(arg->lhs) : NULL;
    var->rhs = arg->rhs ? term_incref(arg->rhs) : NULL;
  } else
    body = beta(body, var, arg, ++*visited);
  term_decref(var), term_decref(arg);
  (term->type = body->type) == TYPE_LAM ? BIND(body->lhs) : 0;
  term->lhs = body->lhs ? term_incref(body->lhs) : NULL;
  term->rhs = body->rhs ? term_incref(body->rhs) : NULL;
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
  // uncomment to disable buffering of user program output
  // fflush(bs->fp);
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
      if (-head->visited != 0)
        return "$exit expects 0 arguments";
      *term = term_decref(*term);
      return NULL;
    case IO_DUMP:
      if (-head->visited != 2)
        return "$dump expects 2 arguments";
      whnf((*term)->lhs->rhs, visited);
      term_dump((*term)->lhs->rhs, ++*visited), fputc('\n', stderr);
      cont = term_incref((*term)->rhs);
      break;
    case IO_GET: {
      if (-head->visited != 1)
        return "$get expects 1 argument";
      bool bit = bs_get(bs_in), eof = bs_eof(bs_in);
      struct term *some, *none, *tru, *fals;
      // clang-format off
      struct term *arg =
          (some = VAR(), LAM(some,
          (none = VAR(), LAM(none,
              eof ? term_incref(none)
                  : APP(term_incref(some),
                        (tru = VAR(), LAM(tru,
                        (fals = VAR(), LAM(fals,
                            term_incref(bit ? tru : fals))))))))));
      // clang-format on
      cont = APP(term_incref((*term)->rhs), arg);
    } break;
    case IO_PUT:
    case IO_EPUT: {
      bool isput = ~head->type == IO_PUT;
      if (-head->visited != 2)
        return isput ? "$put expects 2 arguments" : "$eput expects 2 arguments";
      // two sentinel lambda-terms with bogus `type` so they get treated like
      // IOs and with huge `refcount` so nobody attempts to free them
      struct term tru = {INT_MIN + 1, INT_MAX}, fals = {INT_MIN + 0, INT_MAX};
      struct term *bit = APP(APP(term_incref((*term)->lhs->rhs), &tru), &fals);
      if (~whnf(bit, visited)->type == IO_ERR)
        return term_decref(bit), isput ? "hit $err in $put argument"
                                       : "hit $err in $eput argument";
      if (bit->type != tru.type && bit->type != fals.type)
        return term_decref(bit), isput ? "$put argument is malformed"
                                       : "$eput argument is malformed";
      bs_put(isput ? bs_out : bs_err, bit->type == tru.type), term_decref(bit);
      cont = term_incref((*term)->rhs);
    } break;
    default:
      return "top level is irreducible";
    }
  }
}

// keep in sync with grammar.bnf and pnlc.vim

void parse_ws(char **prog) {
  while (isspace(**prog))
    ++*prog;
}

char *parse_var(char **prog, char **error) {
  if (!**prog || isspace(**prog)) {
    *error = "expected var";
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
    parse_ws(prog);

    struct term *lhs = parse_term(prog, error, env);
    if (*error)
      return NULL;

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

    struct term *lhs = env = term_alloc((struct term){
        TYPE_VAR,
        .lhs = (void *)begin, // binder name
        .rhs = (void *)end,   // end of binder name
        .beta = env,          // next binder up
    });

    struct term *rhs = parse_term(prog, error, env);
    if (*error)
      return term_decref(lhs), NULL;

    // uncomment this to avoid holding any pointers into `prog` when we return.
    // `term_dump` will dump variables as their own memory address instead
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

    // search binders first and IOs second so the IO functions can be shadowed

    for (; env; env = env->beta)
      if (end - begin == (char *)env->rhs - (char *)env->lhs &&
          strncmp(begin, (char *)env->lhs, end - begin) == 0)
        return term_incref(env);

    for (char **io = ios; *io; io++)
      if (end - begin == strlen(*io) && //
          strncmp(begin, *io, end - begin) == 0)
        return IO(io - ios);

    *error = "unbound variable", *prog = begin;
    return NULL;
  }
  }
}

struct term *parse(char **prog, char **error) {
  parse_ws(prog);

  struct term *term = parse_term(prog, error, NULL);
  if (*error)
    return NULL;

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

    buf = realloc(buf, len + *size + 1);
    if (fread(buf + len, 1, *size, fp) != *size)
      perror("fread"), exit(EXIT_FAILURE);
    buf[len += *size] = '\0';
    if (fclose(fp) == EOF)
      perror("fclose"), exit(EXIT_FAILURE);
  }

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
