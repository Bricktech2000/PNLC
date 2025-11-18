#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MK_VAR(IDX) (&(struct term){.lhs = NULL, {.idx = IDX}})
#define MK_LAM(LHS) (&(struct term){.lhs = LHS, {.rhs = NULL}})
#define MK_APP(LHS, RHS) (&(struct term){.lhs = LHS, {.rhs = RHS}})
#define IS_VAR(TERM) !(TERM)->lhs
#define IS_LAM(TERM) ((TERM)->lhs && !(TERM)->u.rhs)
#define IS_APP(TERM) ((TERM)->lhs && (TERM)->u.rhs)

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
    putchar('\\'), term_dump(term->lhs);
  if (IS_VAR(term))
    putchar('0' + term->u.idx);
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
  free((*term)->lhs), free(*term);
  whnf(&body), *term = body;
}

void norm(struct term **term) {
  whnf(term);
  if (IS_APP(*term))
    norm(&(*term)->lhs), norm(&(*term)->u.rhs);
  if (IS_LAM(*term))
    norm(&(*term)->lhs);
}

bool run(struct term *term, FILE *stdin_fp, FILE *stdout_fp) {
  // stdin and stdout are scott-encoded lists of bits, most significant first

  struct term *prog_stdin = NULL, **tail = &prog_stdin;
  for (char c; (c = fgetc(stdin_fp)) != EOF;) {
    for (int i = 0; i < CHAR_BIT; i++) {
      struct term *bit = MK_LAM(MK_LAM(MK_VAR(c >> CHAR_BIT - 1 - i & 1)));
      *tail = term_clone(*MK_LAM(MK_LAM(MK_APP(MK_APP(MK_VAR(1), bit), NULL))));
      tail = &(*tail)->lhs->lhs->u.rhs; // pointer to the `NULL` above
    }
  }
  *tail = term_clone(*MK_LAM(MK_LAM(MK_VAR(0)))); // nil

  struct term *prog_stdout = term_alloc(*MK_APP(term, prog_stdin));
  norm(&prog_stdout), tail = &prog_stdout;

  for (char c; c = 0, true; fputc(c, stdout_fp)) {
    for (int i = 0; i < CHAR_BIT; i++) {
      struct term *t = *tail;
      if (IS_LAM(t) && (t = t->lhs))
        if (IS_LAM(t) && (t = t->lhs))
          if (IS_VAR(t) && t->u.idx == 0 && i == 0)
            goto brk; // nil
          else if (IS_APP(t) && (tail = &t->u.rhs, t = t->lhs))
            if (IS_APP(t) && IS_VAR(t->lhs) && t->lhs->u.idx == 1 &&
                (t = t->u.rhs))
              if (IS_LAM(t) && (t = t->lhs))
                if (IS_LAM(t) && (t = t->lhs))
                  if (IS_VAR(t) && t->u.idx == 0 || t->u.idx == 1) {
                    c <<= 1, c |= t->u.idx;
                    continue;
                  }
      term_free(prog_stdout);
      return false; // stdout malformed
    }
  }
brk:

  term_free(prog_stdout);
  return true;
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
  default:
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
  // XXX hacky
  static char prog[65536] = {0};
  FILE *fp = fopen(argv[1], "r");
  fread(prog, 1, sizeof(prog), fp);

  char *error = NULL, *loc = prog;
  struct term *term = parse(&loc, &error);
  if (error)
    fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc),
        exit(EXIT_FAILURE);

  if (run(term, stdin, stdout) == false)
    fprintf(stderr, "stdout malformed\n"), exit(EXIT_FAILURE);
}
