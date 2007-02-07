#include "stack.h"

sc_val stack_new() {
    return gc_alloc_cons();
}

void stack_push(uint32_t reg_stack, uint32_t reg_val) {
    sc_val new_cell = gc_alloc_cons();

    sc_set_cdr(new_cell, sc_cdr(SC_REG(reg_stack)));
    sc_set_car(new_cell, SC_REG(reg_val));

    sc_set_cdr(SC_REG(reg_stack), new_cell);
}

sc_val stack_pop(uint32_t reg_stack) {
    sc_val v = sc_car(sc_cdr(SC_REG(reg_stack)));
    sc_set_cdr(SC_REG(reg_stack), sc_cdr(sc_cdr(SC_REG(reg_stack))));
    return v;
}

#ifdef BUILD_TEST

#include <stdio.h>
#include <assert.h>

void test_stack() {
    SC_REG(0) = stack_new();

    int i;
    for(i = 0; i < 20; i++) {
        SC_REG(1) = sc_make_number(i);
        stack_push(0, 1);
    }

    sc_val v;
    for(i = 19; i; i--) {
        v = stack_pop(0);
        assert(sc_numberp(v));
        assert(sc_number(v) == i);
    }
}

#endif
