#include "stack.h"

sc_val stack_new() {
    return gc_alloc_cons();
}

void stack_push(sc_val stack, sc_val val) {
    gc_register_roots(&stack, &val, NULL);

    sc_val new_cell = gc_alloc_cons();

    sc_set_cdr(new_cell, sc_cdr(stack));
    sc_set_car(new_cell, val);

    sc_set_cdr(stack, new_cell);

    gc_pop_roots();
}

sc_val stack_pop(sc_val stack) {
    sc_val v = sc_car(sc_cdr(stack));
    sc_set_cdr(stack, sc_cdr(sc_cdr(stack)));
    return v;
}

#ifdef BUILD_TEST

#include <stdio.h>
#include <assert.h>

void test_stack() {
    sc_val stack, val;
    gc_register_roots(&stack, &val, NULL);

    stack = stack_new();

    int i;
    for(i = 0; i < 20; i++) {
        val = sc_make_number(i);
        stack_push(stack, val);
    }

    sc_val v;
    for(i = 19; i; i--) {
        v = stack_pop(stack);
        assert(sc_numberp(v));
        assert(sc_number(v) == i);
    }

    gc_pop_roots();
}

#endif
