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
  size_t len;
  intptr_t v;
  uintptr_t *stk;
  // [0]  : base of stack
  // currently this is the same as the global above
  // however, if we want to operate on different threads, we need to store
  // the stack base in the continuation structure.  Ideally, we wouldn't
  // even calculate it inside here, and instead we'd make the client provide
  // it when they wanted to capture a continuation...
  // [1]  : %rsp
  // [2]  : %rbp
  // [3..]: stack dump, starting with a reference to the continuation struct
};

#if !(defined(__x86_64) || defined(__x86_64__))
# error Only x86_64 architectures is supported
#endif


# define NSAVED 5
# define STACK_PTR(X) __asm__("mov %%rsp, %0" : "=r" (*X))
# define FRAME_PTR(X) __asm__("mov %%rbp, %0" : "=r" (*X))

cont_t cont_new(void) {
  return (cont_t)calloc(1, sizeof(struct cont*));
}

// NB: must compile with -fno-omit-frame-pointer
cont_t cont_capture(cont_t c) {
  ptrdiff_t n;
  uintptr_t *start, *end, *sp1 = (uintptr_t*)stack_base, *sp2, *sp3;
  STACK_PTR(&sp2);
  FRAME_PTR(&sp3);
  n = sp1 - sp2 + 1;
  start = sp2;
  end = sp1;
  if (!c) c = cont_new();
  c->stk = realloc(c, sizeof(*c) + sizeof(uintptr_t) * (3 + NSAVED + n));
  c->len = n+3;
  c->stk[0] = (uintptr_t)sp1;
  c->stk[1] = (uintptr_t)sp2;
  c->stk[2] = (uintptr_t)sp3;
  c->stk[3] = 0;
  __asm__(
    // saved on x86 64
    // stack pointer is saved above
    "movq %%rbx, 0x20(%0);" // frame pointer
    "movq %%r12, 0x28(%0);" // r12-r15
    "movq %%r13, 0x30(%0);"
    "movq %%r14, 0x38(%0);"
    "movq %%r15, 0x40(%0);"
    ::"r"(c->stk)
  );
  memcpy(c->stk+4+NSAVED, start+1, sizeof(intptr_t)*(n-1));
  return c;
}

int cont_throw(cont_t c) {
  uintptr_t *start, *end;
  end = (uintptr_t*)c->stk[0];
  start = (uintptr_t*)c->stk[1];

  __asm__(
    "movq 0x8(%2), %%rsp;" // copy stack ptr to 2nd spot
    "movq 0x10(%2), %%rbp;" // copy frame pointer to 4th spot
    "movq %2, %%rbx;" // rbx = c->stack. used after loop
    "addq $0x48, %2;" // c->stack += 0x48, offset of saved registers
  "copy_stack:"
    "movq (%2), %%rax;" // rax = *c->stack
    "addq $0x8, %0;" // start += 2
    "movq %%rax, (%0);" // *start = rax
    "addq $0x8, %2;" // c->stack += 2
    "cmpq %0, %1;" // if (start != end)
    "jne copy_stack;" // goto copy_stack
    // reset saved registers
    "movq 0x18(%%rbx), %%rax;"
    "movq $0x0, 0x18(%%rbx);"
    "movq 0x28(%%rbx), %%r12;"
    "movq 0x30(%%rbx), %%r13;"
    "movq 0x38(%%rbx), %%r14;"
    "movq 0x40(%%rbx), %%r15;"
    "movq 0x20(%%rbx), %%rbx;"
    "leave;"
    "ret"
    ::"r"(start), "r"(end), "r"(c->stk)
    : "%rax", "%rsp", "%rbx"
  );
  Expect(0, "Unreachable");
  return 0; // silence compiler warning
}

void cont_free(cont_t c) {
  free(c->stk);
  free(c);
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
