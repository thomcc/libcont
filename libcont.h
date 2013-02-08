#ifndef LIBCONT_H
#define LIBCONT_H
#include <inttypes.h>

// opaque continuation structure.
typedef struct cont *cont_t;

#define CONT_T_INITIALIZER NULL

// create a new, empty, continuation.
cont_t cont_new(void);

// store the current continuation in an existing `struct cont`.
// returns the captured continuation the first time. NULL the
// second+ time
cont_t cont_capture(cont_t);

// free the continuation structure and the memory it holds
void cont_free(cont_t);

// invoke the current continuation.
// returns 0 on failure. does not return on success.
int cont_throw(cont_t);

// intialize the library
void cont_init(void);

intptr_t cont_value(cont_t);
void cont_set_value(cont_t, intptr_t);

static inline void cont_vthrow(cont_t c, intptr_t i) {
  cont_set_value(c, i);
  cont_throw(c);
}

#endif