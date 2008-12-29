#ifndef __MINISCHEME_SYMBOL__
#define __MINISCHEME_SYMBOL__

#include "gc.h"

void obarray_init();
gc_handle sc_intern_symbol(char * name);

#endif /* !defined(__MINISCHEME_SYMBOL__) */
