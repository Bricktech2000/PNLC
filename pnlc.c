#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TYPE_APP, TYPE_LAM, TYPE_VAR };

// "IO"s are opaque lambda-terms that are handled in special ways. when `term.
// type < 0`, the term is an IO, and `~term.type` will be one of the following:
enum { IO_EXIT, IO_ERR, IO_GET, IO_PUT, IO_DUMP };
// keep in sync with README.md and pnlc.vim
char *ios[] = {"$exit", "$err", "$get", "$put", "$dump", NULL};

// a `struct term` is a node in a directed acyclic graph. `refcount` is the
// in-degree. `beta` is a borrow, and together with `visited` it forms a cache
// for beta-reduction. for applications, when `type = TYPE_APP`, `lhs` is the
// function and `rhs` is the argument. for abstractions, when `type = TYPE_LAM`,
// `lhs` is the variable and `rhs` is the body. several abstraction nodes might
// bind the same variable node
struct term {
  int type;
  unsigned refcount;
  unsigned long visited;
  struct term *lhs, *rhs;
  struct term *beta;
};

#define APP(LHS, RHS)                                                          \
  term_alloc((struct term){TYPE_APP, .lhs = LHS, .rhs = RHS})
#define LAM(LHS, RHS)                                                          \
  term_alloc((struct term){TYPE_LAM, .lhs = LHS, .rhs = RHS})
#define VAR() term_alloc((struct term){TYPE_VAR})
#define IO(TYP) term_alloc((struct term){~(TYP)})

struct term *term_alloc(struct term fields) {
  struct term *term = malloc(sizeof(*term));
  fields.refcount = 1;
  return *term = fields, term;
}

struct term *term_dump(struct term *term) {
  // fprintf(stderr, "%u|", term->refcount);
  switch (term->type) {
  case TYPE_APP:
    fprintf(stderr, "."), term_dump(term->rhs), term_dump(term->lhs);
    break;
  case TYPE_LAM:
    fprintf(stderr, "\\%p ", (void *)term->lhs), term_dump(term->rhs);
    break;
  case TYPE_VAR:
    fprintf(stderr, "%p ", (void *)term);
    break;
  default:
    fprintf(stderr, "%s ", ios[~term->type]);
    break;
  }
  return term;
}

struct term *term_incref(struct term *term) { return term->refcount++, term; }
struct term *term_decref(struct term *term) {
  // always returns `NULL` so you can go `term = regex_decref(term);`
  if (--term->refcount)
    return NULL;
  if (term->lhs)
    term_decref(term->lhs);
  if (term->rhs)
    term_decref(term->rhs);
  return free(term), NULL;
}

// XXX doc ownership:
//   - `beta` returns owned
//   - `.beta` is a borrow
//   - XXX the two above things are safe because within `beta`, `decref` is only
//     called on owned things with refcount > 1
//   - `beta` moves `term`
//   - `beta` borrows `var` and `arg`

struct term *beta(struct term *term, struct term *var, struct term *arg,
                  unsigned long visited) {
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
      break; // XXX doc
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
  default:
    term->beta = term;
    break;
  }

  term->visited = visited;
  return term->beta; // move out
}

void whnf(struct term *term, unsigned long *visited) {
  // reduce to weak head normal form using normal-order semantics. this means we
  // reduce the leftmost outermost redex first and ignore any redexes inside
  // abstractions or in the argument position of applications. the resulting
  // beta-reduction of `term` is written into `*term` itself so the computation
  // is shared across pointees.

  if (term->type != TYPE_APP)
    return;

  whnf(term->lhs, visited);
  if (term->lhs->type != TYPE_LAM)
    return;

  // we do some gymnastics to make sure `term` doesn't hold a reference to
  // `body` because `beta` can avoid an allocation when its `refcount` is 1.
  struct term *var = term_incref(term->lhs->lhs),
              *body = term_incref(term->lhs->rhs);
  term_decref(term->lhs);
  struct term *rep = beta(body, var, term->rhs, ++*visited);
  term_decref(var), term_decref(term->rhs);
  term->type = rep->type;
  term->lhs = rep->lhs ? term_incref(rep->lhs) : NULL;
  term->rhs = rep->rhs ? term_incref(rep->rhs) : NULL;
  term_decref(rep);
  whnf(term, visited);
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

// XXX doc:

// XXX realization: when doing `whnf`, we can use a `beta` that assumes the
// argument is closed. this simplifies everything. note: crazy, why does no one
// ever mention this? and for `.arg $put`, instead of doing `norm(arg)` then
// pattern-matching on the result, we can do `whnf(app(app(norm, $true),
// $false))` and check whether the result is `$true` or `$false`. no need for
// `norm`.

char *run(struct term **term, struct bs *bs_in, struct bs *bs_out,
          unsigned long *visited) {
  // takes ownership of `*term`. upon successful termination, returns `NULL` and
  // writes `NULL` into `*term`; otherwise, returns an error message and stores
  // the problematic term into `*term`

  for (struct term *cont;; term_decref(*term), *term = cont) {
    whnf(*term, visited);
    int argc = 0;
    struct term *io = *term;
    while (io->type == TYPE_APP)
      io = io->lhs, argc++;
    if (io->type >= 0)
      return "top-level term is not IO";

    switch (~io->type) {
    case IO_ERR:
      return "top-level term is $err";
    case IO_EXIT:
      if (argc != 0)
        return "$exit expects 0 arguments";
      *term = term_decref(*term);
      return NULL;
    case IO_DUMP:
      if (argc != 2)
        return "$dump expects 2 arguments";
      whnf((*term)->lhs->rhs, visited);
      term_dump((*term)->lhs->rhs), putchar('\n');
      cont = term_incref((*term)->rhs);
      break;
    case IO_GET: {
      if (argc != 1)
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
    case IO_PUT: {
      if (argc != 2)
        return "$put expects 2 arguments";
      // two sentinel lambda-terms with bogus `type` so they get treated like
      // IOs and with huge `refcount` so nobody attempts to free them
      struct term tru = {INT_MIN + 1, INT_MAX}, fals = {INT_MIN + 0, INT_MAX};
      struct term *bit = APP(APP(term_incref((*term)->lhs->rhs), &tru), &fals);
      whnf(bit, visited);
      if (bit->type != tru.type && bit->type != fals.type)
        return term_decref(bit), "$put argument is malformed";
      bs_put(bs_out, bit->type == tru.type), term_decref(bit);
      cont = term_incref((*term)->rhs);
    } break;
    default:
      abort();
    }
  }
}

// keep in sync with grammar.bnf and pnlc.vim

struct env {
  char *begin, *end; // binder name
  struct term *var;  // borrow of bound variable node
  struct env *up;    // next binder up the list
};

void parse_ws(char **prog) {
  while (isspace(**prog))
    ++*prog;
}

char *parse_var(char **prog, char **error) {
  if (!isgraph(**prog)) {
    *error = "expected var";
    return NULL;
  }

  while (isgraph(**prog))
    ++*prog;

  char *end = (*prog)++;
  parse_ws(prog);
  return end;
}

struct term *parse_term(char **prog, char **error, struct env *env) {
  if (**prog == '\0') {
    *error = "expected term";
    return NULL;
  }

  switch (*(*prog)++) {
  case '.': {
    parse_ws(prog);

    struct term *rhs = parse_term(prog, error, env);
    if (*error)
      return NULL;

    struct term *lhs = parse_term(prog, error, env);
    if (*error)
      return term_decref(rhs), NULL;

    return APP(lhs, rhs);
  }
  case '\\': {
    parse_ws(prog);

    char *begin = *prog, *end = parse_var(prog, error);
    if (*error)
      return NULL;

    // allocate new binder
    struct term *lhs = VAR();
    env = &(struct env){.begin = begin, .end = end, .var = lhs, .up = env};

    struct term *rhs = parse_term(prog, error, env);
    if (*error)
      return term_decref(lhs), NULL;

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

    // check if the variable is an IO
    for (char **io = ios; *io; io++)
      if (end - begin == strlen(*io) && strncmp(begin, *io, end - begin) == 0)
        return IO(io - ios);

    // else find corresponding binder
    for (; env; env = env->up)
      if (end - begin == env->end - env->begin &&
          strncmp(begin, env->begin, end - begin) == 0)
        return term_incref(env->var);

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

  if (**prog != '\0') {
    *error = "trailing characters";
    return term_decref(term), NULL;
  }

  return term;
}

int main(int argc, char **argv) {
  if (argc <= 1)
    fputs("usage: pnlc <files...>\n", stderr), exit(EXIT_FAILURE);

  char *buf = NULL;
  size_t len = 0;

  while (*++argv) {
    long size;
    FILE *fp = fopen(*argv, "r");
    if (fp == NULL)
      perror("fopen"), exit(EXIT_FAILURE);
    if (fseek(fp, 0, SEEK_END) == -1)
      perror("fseek"), exit(EXIT_FAILURE);
    if ((size = ftell(fp)) == -1)
      perror("ftell"), exit(EXIT_FAILURE);
    rewind(fp);

    buf = realloc(buf, len + size + 1);
    if (fread(buf + len, 1, size, fp) != size)
      perror("fread"), exit(EXIT_FAILURE);
    buf[len += size] = '\0';
    if (fclose(fp) == EOF)
      perror("fclose"), exit(EXIT_FAILURE);
  }

  char *error = NULL, *loc = buf;
  struct term *term = parse(&loc, &error);
  if (error)
    fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc),
        exit(EXIT_FAILURE);

  free(buf);

  if (error = run(&term, &(struct bs){stdin}, &(struct bs){stdout},
                  &(unsigned long){0}))
    term_decref(term), fprintf(stderr, "runtime error: %s\n", error),
        exit(EXIT_FAILURE);
}
