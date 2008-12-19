#include "symbol.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#define OBARRAY_INITIAL_SIZE 50

sc_val obarray;

void obarray_init() {
    obarray = gc_alloc_vector(OBARRAY_INITIAL_SIZE);
    gc_register_roots(&obarray, NULL);
}

sc_val sc_intern_symbol(char * name) {
    int i;
    uint32_t len = sc_vector_len(obarray);
    sc_val v = NIL;
    for(i = 0; i < len; i++) {
        v = sc_vector_ref(obarray, i);
        if(NILP(v)) break;
        if(!strcmp(name, sc_symbol_name(v)))
            return v;
    }
    /* Symbol not found, allocate one and stick it in the obarray */
    if( i == len) {
        /* We ran off the end -- realloc the obarray */
        printf("Obarray realloc forced, new size %d\n", len << 1);
        sc_val oa = gc_alloc_vector(len << 1);
        for(i = 0; i < len; i++) {
            sc_vector_set(oa, i, sc_vector_ref(obarray,i));
        }
        obarray = oa;
        i = len;
    }
    v = gc_alloc_symbol(strlen(name));
    strcpy(sc_symbol_name(v), name);
    sc_vector_set(obarray, i, v);
    return v;
}

#ifdef BUILD_TEST

void test_symbol(void) {
    sc_val r0, r1;
    char str[2] = "a";
    int i;

    gc_register_roots(&r0, &r1, NULL);

    r0 = sc_intern_symbol("hello");
    r1 = sc_intern_symbol("hello");

    assert(!strcmp(sc_symbol_name(r0), "hello"));
    assert(r0 == r1);

    /* Intern a lot of junk to force an obarray realloc */
    for(i = 0; i < 26; i++) {
        str[0] = 'a' + i;
        sc_intern_symbol(str);
        str[0] = 'A' + i;
        sc_intern_symbol(str);
    }

    assert(!strcmp(sc_symbol_name(r0), "hello"));
    assert(r0 == r1);
}

#endif
