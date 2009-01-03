#include <string.h>

#include "scgc.h"

#define STRLEN2CELLS(x) (ROUNDUP(((uint32_t)(x)),sizeof(gc_handle))/sizeof(gc_handle))

gc_handle sc_true, sc_false;

/* Types */
typedef struct sc_cons {
    gc_chunk header;
    gc_handle   car;
    gc_handle   cdr;
} sc_cons;

typedef struct sc_string {
    gc_chunk header;
    uint32_t strlen;
    char     string[];
} sc_string;

typedef sc_string sc_symbol;

typedef struct sc_vector {
    gc_chunk header;
    uint32_t veclen;
    gc_handle vector[];
} sc_vector;

typedef struct sc_boolean {
    gc_chunk header;
    int      val;
} sc_boolean;

/* Op functions */

uint32_t sc_len_string(gc_chunk *v) {
    return 1 + STRLEN2CELLS(((sc_string*)v)->strlen);
}

uint32_t sc_len_cons(gc_chunk *v UNUSED) {
    return 2;
}

uint32_t sc_len_vector(gc_chunk *v) {
    return 1 + ((sc_vector*)v)->veclen;
}

uint32_t sc_len_boolean(gc_chunk *v UNUSED) {
    return 1;
}

void sc_relocate_cons(gc_chunk *v) {
    sc_cons *cons = (sc_cons*)v;
    gc_relocate(&cons->car);
    gc_relocate(&cons->cdr);
}

void sc_relocate_vector(gc_chunk *v) {
    uint32_t i;
    sc_vector *vec = (sc_vector*)v;

    for(i = 0; i < vec->veclen; i++) {
        gc_relocate(&vec->vector[i]);
    }
}

/* Op structs */
static struct gc_ops sc_symbol_ops = {
    .op_relocate = gc_relocate_nop,
    .op_len      = sc_len_string
};

static struct gc_ops sc_string_ops = {
    .op_relocate = gc_relocate_nop,
    .op_len      = sc_len_string
};

static struct gc_ops sc_cons_ops = {
    .op_relocate = sc_relocate_cons,
    .op_len      = sc_len_cons
};

static struct gc_ops sc_vector_ops = {
    .op_relocate = sc_relocate_vector,
    .op_len      = sc_len_vector
};

static struct gc_ops sc_boolean_ops = {
    .op_relocate = gc_relocate_nop,
    .op_len      = sc_len_boolean
};

/* Public API */

gc_handle sc_car(gc_handle c) {
    assert(sc_consp(c));
    return UNTAG_PTR(c, sc_cons)->car;
}

gc_handle sc_cdr(gc_handle c) {
    assert(sc_consp(c));
    return UNTAG_PTR(c, sc_cons)->cdr;
}

void sc_set_car(gc_handle c, gc_handle val) {
    assert(sc_consp(c));
    UNTAG_PTR(c, sc_cons)->car = val;
}

void sc_set_cdr(gc_handle c, gc_handle val) {
    assert(sc_consp(c));
    UNTAG_PTR(c, sc_cons)->cdr = val;
}

char * sc_string_get(gc_handle c) {
    assert(sc_stringp(c));
    return UNTAG_PTR(c, sc_string)->string;
}

char * sc_symbol_name(gc_handle c) {
    assert(sc_symbolp(c));
    return UNTAG_PTR(c, sc_symbol)->string;
}

uint32_t sc_strlen(gc_handle c) {
    assert(sc_stringp(c));
    return UNTAG_PTR(c, sc_string)->strlen;
}

gc_int sc_number(gc_handle n) {
    assert(sc_numberp(n));
    return gc_untag_number(n);
}

gc_handle sc_make_number(gc_int n) {
    return gc_tag_number(n);
}

uint32_t sc_vector_len(gc_handle v) {
    assert(sc_vectorp(v));
    return UNTAG_PTR(v, sc_vector)->veclen;
}

gc_handle sc_vector_ref(gc_handle v, uint32_t n) {
    assert(sc_vectorp(v));
    assert(n < sc_vector_len(v));
    return UNTAG_PTR(v, sc_vector)->vector[n];
}

void sc_vector_set(gc_handle v, uint32_t n, gc_handle x) {
    assert(sc_vectorp(v));
    assert(n < sc_vector_len(v));
    UNTAG_PTR(v, sc_vector)->vector[n] = x;
}

/* Predicates */
static inline int sc_pointer_typep(gc_handle c, gc_ops *type) {
    return gc_pointerp(c)
        && !NILP(c)
        && UNTAG_PTR(c, gc_chunk)->ops == type;
}

int sc_consp(gc_handle c) {
    return sc_pointer_typep(c, &sc_cons_ops);
}

int sc_stringp(gc_handle c) {
    return sc_pointer_typep(c, &sc_string_ops);
}

int sc_symbolp(gc_handle c) {
    return sc_pointer_typep(c, &sc_symbol_ops);
}

int sc_vectorp(gc_handle c) {
    return sc_pointer_typep(c, &sc_vector_ops);
}

int sc_booleanp(gc_handle c) {
    return sc_pointer_typep(c, &sc_boolean_ops);
}

int sc_numberp(gc_handle c) {
    return gc_numberp(c);
}

/* Memory allocation */

gc_handle sc_alloc_cons() {
    sc_cons *cons = (sc_cons*)gc_alloc(&sc_cons_ops, 3);
    cons->car = cons->cdr = NIL;
    return gc_tag_pointer(cons);
}

gc_handle sc_alloc_string(uint32_t len) {
    sc_string *str = (sc_string*)gc_alloc(&sc_string_ops, STRLEN2CELLS(len) + 2);
    str->strlen = len;
    return gc_tag_pointer(str);
}

gc_handle sc_alloc_vector(uint32_t len) {
    sc_vector *vec = (sc_vector*)gc_alloc(&sc_vector_ops, len + 2);
    int i;
    vec->veclen = len;
    for(i = 0; i < len; i++) {
        vec->vector[i] = NIL;
    }
    return gc_tag_pointer(vec);
}

gc_handle sc_alloc_symbol(uint32_t len) {
    sc_symbol *sym = (sc_string*)gc_alloc(&sc_symbol_ops, STRLEN2CELLS(len) + 2);
    sym->strlen = len;
    return gc_tag_pointer(sym);
}

gc_handle sc_make_string(char *string) {
    uint32_t len = strlen(string);
    gc_handle s = sc_alloc_string(len+1);
    strcpy(sc_string_get(s), string);
    return s;
}

void sc_init() {
    sc_true = gc_tag_pointer(gc_alloc(&sc_boolean_ops, 2));
    UNTAG_PTR(sc_true, sc_boolean)->val = 1;
    sc_false = gc_tag_pointer(gc_alloc(&sc_boolean_ops, 2));
    UNTAG_PTR(sc_false, sc_boolean)->val = 0;
}
