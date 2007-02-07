#ifndef __MINISCHEME_SYMBOL__
#define __MINISCHEME_SYMBOL__

#include "gc.h"

#define OBARRAY (SC_NREGS - 2)

void obarray_init();
sc_val sc_intern_symbol(char * name);

#endif /* !defined(__MINISCHEME_SYMBOL__) */
