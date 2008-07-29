#ifndef __MINISCHEME_STACK__
#define __MINISCHEME_STACK__

#include "gc.h"

sc_val stack_new();

void stack_push(sc_val stack, sc_val val);
sc_val stack_pop(sc_val stack);

#endif
