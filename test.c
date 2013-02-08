#include <stdlib.h>
#include <stdio.h>
#include "libcont.h"

static cont_t fact_c = CONT_T_INITIALIZER;

int fact(int n) {
  if (n == 0) {
    cont_t v = cont_capture(fact_c);
    if (v) { fact_c = v; return 1; }
    else return (int)cont_value(fact_c);
  } else {
    printf("Going down (n=%d)\n", n);
    return n * fact(n-1);
  }
}

int do_fact() {
  int y = fact(5);
  printf("y = %d\n", y);
  cont_vthrow(fact_c, 1); // works with 2 also.
  return 0; // never happens
}

int main(int argc, char* argv[]) {
  cont_init();
  do_fact();
  exit(0);
}
