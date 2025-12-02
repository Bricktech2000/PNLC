#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *ios[] = {"", "err", "get", "put", "dbg", NULL};
enum {
  IO_END,
  IO_ERR,
  IO_GET,
  IO_PUT,
  IO_DBG,
};

#define MK_IO(IO) (&(struct term){.lhs = NULL, {.idx = ~(IO)}})
#define MK_VAR(IDX) (&(struct term){.lhs = NULL, {.idx = IDX}})
#define MK_LAM(LHS) (&(struct term){.lhs = LHS, {.rhs = NULL}})
#define MK_APP(LHS, RHS) (&(struct term){.lhs = LHS, {.rhs = RHS}})
#define IS_IO(TERM) (!(TERM)->lhs && (TERM)->u.idx < 0)
#define IS_VAR(TERM) (!(TERM)->lhs && (TERM)->u.idx >= 0)
#define IS_LAM(TERM) ((TERM)->lhs && !(TERM)->u.rhs)
#define IS_APP(TERM) ((TERM)->lhs && (TERM)->u.rhs)
#define IO_TYP(TERM) ~(TERM)->u.idx

struct term {
  struct term *lhs;
  union {
    struct term *rhs;
    int idx;
  } u;
};

struct term *term_alloc(struct term fields) {
  struct term *term = malloc(sizeof(*term));
  return *term = fields, term;
}

struct term *term_clone(struct term term) {
  if (term.lhs)
    term.lhs = term_clone(*term.lhs);
  if (term.lhs && term.u.rhs)
    term.u.rhs = term_clone(*term.u.rhs);
  return term_alloc(term);
}

struct term *term_dump(struct term *term) {
  if (IS_APP(term))
    putchar('.'), term_dump(term->u.rhs), term_dump(term->lhs);
  if (IS_LAM(term))
    putchar('\\'), putchar(' '), term_dump(term->lhs);
  if (IS_VAR(term))
    printf("%d ", term->u.idx);
  if (IS_IO(term))
    printf("$%s ", ios[IO_TYP(term)]);
  return term;
}

void term_free(struct term *term) {
  if (term->lhs)
    term_free(term->lhs);
  if (term->lhs && term->u.rhs)
    term_free(term->u.rhs);
  free(term);
}

void beta(struct term **term, struct term *arg, int idx, int *copies) {
  if (IS_APP(*term))
    beta(&(*term)->lhs, arg, idx, copies),
        beta(&(*term)->u.rhs, arg, idx, copies);

  if (IS_LAM(*term))
    beta(&(*term)->lhs, arg, idx + 1, copies);

  if (IS_VAR(*term)) {
    if ((*term)->u.idx > idx) // free variable
      (*term)->u.idx--;
    else if ((*term)->u.idx < idx) // bound variable
      ;
    else if ((*copies)++ == 0) // first substitution
      free(*term), *term = arg;
    else // subsequent substitutions
      free(*term), *term = term_clone(*arg);
  }
}

void whnf(struct term **term) {
  if (!IS_APP(*term))
    return;

  whnf(&(*term)->lhs);
  if (!IS_LAM((*term)->lhs))
    return;

  int copies = 0;
  struct term *body = (*term)->lhs->lhs;
  beta(&body, (*term)->u.rhs, 0, &copies);
  if (copies == 0)
    term_free((*term)->u.rhs);
  free((*term)->lhs), free(*term), *term = body;
  whnf(term);
}

void norm(struct term **term) {
  whnf(term);
  if (IS_APP(*term))
    norm(&(*term)->lhs), norm(&(*term)->u.rhs);
  if (IS_LAM(*term))
    norm(&(*term)->lhs);
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
  *term = term_alloc(*MK_APP(*term, term_alloc(*MK_IO(IO_END))));

  while (1) {
    whnf(term);
    int args = 0;
    struct term *io = *term;
    while (IS_APP(io))
      io = io->lhs, args++;
    if (!IS_IO(io))
      return "top-level term is not io";

    if (IO_TYP(io) == IO_ERR) {
      return "top-level term is err";
    }
    if (IO_TYP(io) == IO_END && args == 0) {
      free(*term), *term = NULL;
      return NULL;
    }
    if (IO_TYP(io) == IO_DBG && args == 2) {
      norm(&(*term)->lhs->u.rhs);
      term_dump((*term)->lhs->u.rhs), putchar('\n');
      goto hoist;
    }
    if (IO_TYP(io) == IO_GET && args == 1) {
      bool bit = bs_get(bs_in), eof = bs_eof(bs_in);
      struct term *arg = term_clone(*MK_LAM(MK_LAM(
          eof ? MK_VAR(0) : MK_APP(MK_VAR(1), MK_LAM(MK_LAM(MK_VAR(bit)))))));
      (*term)->u.rhs = term_alloc(*MK_APP((*term)->u.rhs, arg));
      goto hoist;
    }
    if (IO_TYP(io) == IO_PUT && args == 2) {
      norm(&(*term)->lhs->u.rhs);
      struct term *arg = ((*term)->lhs->u.rhs);
      if (!IS_LAM(arg) || !IS_LAM(arg->lhs) || !IS_VAR(arg->lhs->lhs))
        return IS_IO(arg) && IO_TYP(arg) == IO_ERR ? "io argument is err"
                                                   : "io argument is malformed";
      bs_put(bs_out, (*term)->lhs->u.rhs->lhs->lhs->u.idx);
      goto hoist;
    }

    return "io has wrong argument count";

  hoist:;
    struct term *cont = (*term)->u.rhs;
    term_free((*term)->lhs), free(*term), *term = cont;
  }
}

struct env {
  char *begin, *end; // binder name
  struct env *up;    // next binder up the list
};

void parse_ws(char **prog, char **error) {
redo:
  while (isspace(**prog))
    ++*prog;

  if (**prog == '#') {
    char *nl = strchr(*prog, '\n');
    if (nl == NULL) {
      *error = "unterminated comment";
      return;
    }

    *prog = nl + 1;
    goto redo;
  }
}

char *parse_var(char **prog, char **error) {
  parse_ws(prog, error);
  if (*error)
    return NULL;

  if (!isgraph(**prog)) {
    *error = "expected var";
    return NULL;
  }

  char *var = *prog;
  while (isgraph(**prog))
    ++*prog;
  return var;
}

struct term *parse_term(char **prog, char **error, struct env *env) {
  parse_ws(prog, error);
  if (*error)
    return NULL;

  if (**prog == '\0') {
    *error = "expected term";
    return NULL;
  }

  switch (*(*prog)++) {
  case '.': {
    struct term *rhs = parse_term(prog, error, env);
    if (*error)
      return NULL;

    struct term *lhs = parse_term(prog, error, env);
    if (*error)
      return term_free(rhs), NULL;

    return term_alloc(*MK_APP(lhs, rhs));
  }
  case '\\': {
    char *var = parse_var(prog, error);
    if (*error)
      return NULL;

    // allocate new binder
    env = &(struct env){.begin = var, .end = *prog, .up = env};

    struct term *lhs = parse_term(prog, error, env);
    if (*error)
      return NULL;

    return term_alloc(*MK_LAM(lhs));
  }
  case '$': {
    char *var = parse_var(prog, error);
    if (*error)
      return NULL;

    for (char **io = ios; *io; io++)
      if (strncmp(var, *io, *prog - var) == 0)
        return term_alloc(*MK_IO(io - ios));

    *error = "unknown io", *prog = var;
    return NULL;
  }
  default: {
    --*prog;
    char *var = parse_var(prog, error);
    if (*error)
      return NULL;

    // find corresponding binder
    for (int idx = 0; env; env = env->up, idx++)
      if (*prog - var == env->end - env->begin &&
          strncmp(var, env->begin, *prog - var) == 0)
        return term_alloc(*MK_VAR(idx));

    *error = "unbound variable", *prog = var;
    return NULL;
  }
  }
}

struct term *parse(char **prog, char **error) {
  struct term *term = parse_term(prog, error, NULL);
  if (*error)
    return NULL;

  parse_ws(prog, error);
  if (*error)
    return term_free(term), NULL;

  if (**prog != '\0') {
    *error = "trailing characters";
    return term_free(term), NULL;
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

  if (error = run(&term, &(struct bs){stdin}, &(struct bs){stdout}))
    term_free(term), fprintf(stderr, "runtime error: %s\n", error),
        exit(EXIT_FAILURE);
}
