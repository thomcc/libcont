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
  // loosely based on http://www.scs.stanford.edu/histar/src/lind/os-Linux/setjmp.S
  // and https://github.com/promovicz/dietlibc/blob/master/x86_64/setjmp.S
  __asm__(
    // saved by setjmp on x86 64 according to header file.
    // rbp (frame) is explicitly c->stk[1]
    /* rip, rbp, rsp, rbx, r12, r13, r14, r15 */
    // stack pointer is saved above
    "movq %%rbx, 0x20(%0);" // frame pointer
    "movq %%r12, 0x28(%0);" // r12-r15 see links above
    "movq %%r13, 0x30(%0);"
    "movq %%r14, 0x38(%0);"
    "movq %%r15, 0x40(%0);"
    ::"r"(c->stk)
    // none clobbered
  );
  memcpy(c->stk+4+NSAVED, start+1, sizeof(intptr_t)*(n-1));
  return c;
}

int cont_throw(cont_t c) {
  uintptr_t *start, *end;
  end = (uintptr_t*)c->stk[0];
  start = (uintptr_t*)c->stk[1];
  // loosely based on http://www.scs.stanford.edu/histar/src/lind/os-Linux/setjmp.S
  // and https://github.com/promovicz/dietlibc/blob/master/x86_64/setjmp.S
  __asm__(
    "movq 0x8(%2), %%rsp;"   // restore stack pointer
    "movq 0x10(%2), %%rbp;"  // restore freame pointer
    "movq %2, %%rbx;"        // rbx = c->stack
    "addq $0x48, %2;"         // c->stack += 9, offset from saved registers
  "copy_stack:"              // copy the stack from %2 (c->stack) to %0 (start)
    "movq (%2), %%rax;"      // rax <- *stack
    "addq $0x8, %0;"         // ++start; (interleaved)
    "movq %%rax, (%0);"      // *start <- rax;
    "addq $0x8, %2;"         // ++c->stack;
    "cmpq %0, %1;"           // if (start != end)
    "jne copy_stack;"        //   goto copy_stack
    "movq 0x18(%%rbx), %%rax;"
    // restore bx
    "movq $0x0, 0x18(%%rbx);" // restore previous
    // restore regs 12-15, see links above
    // and assembly block in capture_cont for (same) offsets
    "movq 0x28(%%rbx), %%r12;"
    "movq 0x30(%%rbx), %%r13;"
    "movq 0x38(%%rbx), %%r14;"
    "movq 0x40(%%rbx), %%r15;"
    "movq 0x20(%%rbx), %%rbx;" // needs to be last
    "leave;"
    "ret" // get return address from stack (which we modified) and set rip
    // avoid overwriting our stack frame as we use it
    ::"r"(start), "r"(end), "r"(c->stk)
    // clobbered rax, rbx, and stack pointer.
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
