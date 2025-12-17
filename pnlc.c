#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TYPE_APP, TYPE_LAM, TYPE_VAR, TYPE_IO };
char *ios[] = {"$end", "$err", "$get", "$put", "$dbg", NULL};
enum { IO_END, IO_ERR, IO_GET, IO_PUT, IO_DBG };

#define APP(LHS, RHS)                                                          \
  term_alloc((struct term){TYPE_APP, .lhs = LHS, .rhs = RHS})
#define LAM(LHS, RHS)                                                          \
  term_alloc((struct term){TYPE_LAM, .lhs = LHS, .rhs = RHS})
#define VAR() term_alloc((struct term){TYPE_VAR})
#define IO(TYP) term_alloc((struct term){TYPE_IO + TYP})

struct term {
  int type;
  unsigned refcount;
  struct term *lhs, *rhs;
  struct term *beta;
};

struct term *term_alloc(struct term fields) {
  struct term *term = malloc(sizeof(*term));
  fields.refcount = 1;
  return *term = fields, term;
}

struct term *term_dump(struct term *term) {
  // printf("%u|", term->refcount);
  switch (term->type) {
  case TYPE_APP:
    putchar('.'), term_dump(term->rhs), term_dump(term->lhs);
    break;
  case TYPE_LAM:
    printf("\\%p ", (void *)term->lhs), term_dump(term->rhs);
    break;
  case TYPE_VAR:
    printf("%p ", (void *)term);
    break;
  default: // TYPE_IO
    printf("%s ", ios[term->type - TYPE_IO]);
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

void unmark(struct term *term) {
  if (!term->beta || (term->beta = NULL))
    return;
  if (term->lhs)
    unmark(term->lhs);
  if (term->rhs)
    unmark(term->rhs);
}

// XXX doc ownership:
//   - `beta` returns owned
//   - `.beta` is a borrow
//   - XXX the two above things are safe because within `beta`, `decref` is only
//     called on owned things with refcount > 1
//   - `beta` borrows `term`
//   - `beta` borrows `var` and `arg`
// XXX opt no need to alloc when refcount is 1
// XXX opt there would be no need to `unmark` if we had a `long visited`

struct term *beta(struct term *term, struct term *var, struct term *arg) {
  if (term->beta)
    return term_incref(term->beta);

  switch (term->type) {
  case TYPE_APP: {
    struct term *lhs = beta(term->lhs, var, arg);
    struct term *rhs = beta(term->rhs, var, arg);
    if (lhs == term->lhs && rhs == term->rhs)
      term_decref(lhs), term_decref(rhs), term->beta = term_incref(term);
    else
      term->beta = APP(lhs, rhs);
  } break;
  case TYPE_LAM: {
    if (term->lhs == var) {
      term->beta = term_incref(term);
      break; // XXX doc
    }
    struct term *rhs = beta(term->rhs, var, arg);
    if (rhs == term->rhs)
      term_decref(rhs), term->beta = term_incref(term);
    else
      term->beta = LAM(term_incref(term->lhs), rhs);
  } break;
  case TYPE_VAR:
    term->beta = term_incref(term == var ? arg : term);
    break;
  default: // TYPE_IO
    term->beta = term_incref(term);
    break;
  }

  return term->beta; // XXX doc move ownership
}

void whnf(struct term **term) {
  if ((*term)->type != TYPE_APP)
    return;

  whnf(&(*term)->lhs);
  if ((*term)->lhs->type != TYPE_LAM)
    return;

  struct term *body = (*term)->lhs->rhs;
  body = beta(body, (*term)->lhs->lhs, (*term)->rhs);
  unmark((*term)->lhs->rhs);
  term_decref(*term), *term = body;
  whnf(term);
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

char *run(struct term **term, struct bs *bs_in, struct bs *bs_out) {
  *term = APP(*term, IO(IO_END));

  for (struct term *cont;; term_decref(*term), *term = cont) {
    whnf(term);
    int args = 0;
    struct term *io = *term;
    while (io->type == TYPE_APP)
      io = io->lhs, args++;
    if (io->type < TYPE_IO)
      return "top-level term is not io";

    switch (io->type - TYPE_IO) {
    case IO_ERR:
      return "top-level term is err";
    case IO_END:
      if (args != 0)
        return "$end expects 0 arguments";
      term_decref(*term), *term = NULL;
      return NULL;
    case IO_DBG:
      if (args != 2)
        return "$dbg expects 2 arguments";
      whnf(&(*term)->lhs->rhs);
      term_dump((*term)->lhs->rhs), putchar('\n');
      cont = term_incref((*term)->rhs);
      break;
    case IO_GET: {
      if (args != 1)
        return "$get expects 1 argument";
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
    case IO_PUT: {
      if (args != 2)
        return "$put expects 2 arguments";
      struct term *one = VAR(), *zero = VAR();
      struct term *bit = APP(APP(term_incref((*term)->lhs->rhs), one), zero);
      whnf(&bit);
      if (bit != one && bit != zero)
        return term_decref(bit), "io argument is malformed";
      bs_put(bs_out, bit == one), free(bit);
      cont = term_incref((*term)->rhs);
    } break;
    }
  }
}

// keep in sync with grammar.bnf

struct env {
  char *begin, *end; // binder name
  struct term *var;  // XXX doc
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
    for (char **io = ios + 1 /* IO_END is reserved */; *io; io++)
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

  if (error = run(&term, &(struct bs){stdin}, &(struct bs){stdout}))
    term_decref(term), fprintf(stderr, "runtime error: %s\n", error),
        exit(EXIT_FAILURE);
}
