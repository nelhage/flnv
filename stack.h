#ifndef __MINISCHEME_STACK__
#define __MINISCHEME_STACK__

#include "gc.h"

#define STACK (SC_NREGS - 1)

sc_val stack_new();

void stack_push(uint32_t reg_stack, uint32_t reg_val);
sc_val stack_pop(uint32_t reg_stack);

#endif
