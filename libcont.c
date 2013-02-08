#include <setjmp.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libcont.h"

#define Expect(X, M) do {if (!(X)){fputs("error: " M "\n", stderr); abort();}} while (0)

static char *stack_base = 0;

struct cont {
  jmp_buf jb;
  size_t len;
  intptr_t v;
  intptr_t *stk;
};

cont_t cont_new(void) {
  return (cont_t)calloc(1, sizeof(struct cont*));
}

cont_t cont_capture(cont_t c) {
  ptrdiff_t stacklen;
  char *stack_top = (char*)&stacklen;
  if (!c) c = cont_new();
  assert(stack_base && "cont_capture called before CONT_INIT()!");
  if (setjmp(c->jb)) return 0;

  stacklen = stack_base - stack_top;
  assert(stacklen > 0 && "Unsupported architecture: stack must grow down.");

  c->len = stacklen;
  c->stk = realloc(c->stk, c->len);
  Expect(c->stk != NULL, "Virtual memory exhausted");
  memcpy(c->stk, stack_top, c->len);
  return c;
}

void cont_free(cont_t c) {
  free(c->stk);
  free(c);
}

static void more_stack(cont_t c) {
  volatile intptr_t dummy[128]; (void)dummy; // silence compiler warning
  cont_throw(c);
}

int cont_throw(cont_t c) {
  intptr_t stack_top;
  if (stack_base - c->len < (char*)&stack_top) more_stack(c);

  Expect(stack_base - c->len >= (char*)&stack_top, "Compiler overly eager?");

  memcpy(stack_base-c->len, c->stk, c->len);

  longjmp(c->jb, 1);
  Expect(0, "throw failed?");
  return 0; // shouldn't ever get here
}

void cont_init(void) {
  volatile intptr_t base;
  stack_base = (char*)&base;
}

intptr_t cont_value(cont_t c) {
  assert(c != NULL);
  return c->v;
}

void cont_set_value(cont_t c, intptr_t v) {
  assert(c != NULL);
  c->v = v;
}
