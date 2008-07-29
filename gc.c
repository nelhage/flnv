#include "gc.h"
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

/* Constants */
#define GC_INITIAL_MEM  1024

sc_val _sc_regs[SC_NREGS];

static sc_val sc_root;

static uintptr_t * working_mem;
static uintptr_t * free_mem;
static uintptr_t * free_ptr;
static uintptr_t mem_size;

#define TAG_NUMBER(x)  (uint32_t)((x)<<1 | 0x1)
#define TAG_POINTER(x) ((sc_val)(x))

#define NUMBERP(x) ((uintptr_t)(x)&0x1)
#define POINTERP(x)  (!NUMBERP(x))

#define UNTAG_PTR(c, t) ((t*)c)
#define UNTAG_NUMBER(c) (((sc_int)(c))>>1)

#define MAX(a,b) (((a)>(b))?(a):(b))

#define STRLEN2CELLS(x) (ROUNDUP(((uint32_t)x),sizeof(sc_val))/sizeof(sc_val))

// Rounding operations (efficient when n is a power of 2)
// Round down to the nearest multiple of n
#define ROUNDDOWN(a, n)                                         \
({                                                              \
        uint32_t __a = (uint32_t) (a);                          \
        (typeof(a)) (__a - __a % (n));                          \
})
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)                                           \
({                                                              \
        uint32_t __n = (uint32_t) (n);                          \
        (typeof(a)) (ROUNDDOWN((uint32_t) (a) + __n - 1, __n)); \
})


#define TYPE_SYMBOL        1
#define TYPE_STRING        2
#define TYPE_CONS          3
#define TYPE_VECTOR        4
#define TYPE_BROKEN_HEART  ((uint8_t)-1)

typedef struct gc_chunk {
    uint8_t type;
    uint32_t extra : 24;
    sc_val  data[0];
} gc_chunk;

typedef struct gc_cons {
    gc_chunk header;
    sc_val   car;
    sc_val   cdr;
} gc_cons;

typedef struct gc_symbol {
    gc_chunk header;
    char name[0];
} gc_symbol;

typedef struct gc_string {
    gc_chunk header;
    char string[0];
} gc_string;

typedef struct gc_vector {
    gc_chunk header;
    sc_val   vector[0];
} gc_vector;

sc_val sc_car(sc_val c) {
    assert(sc_consp(c));
    return UNTAG_PTR(c, gc_cons)->car;
}

sc_val sc_cdr(sc_val c) {
    assert(sc_consp(c));
    return UNTAG_PTR(c, gc_cons)->cdr;
}

void sc_set_car(sc_val c, sc_val val) {
    assert(sc_consp(c));
    UNTAG_PTR(c, gc_cons)->car = val;
}

void sc_set_cdr(sc_val c, sc_val val) {
    assert(sc_consp(c));
    UNTAG_PTR(c, gc_cons)->cdr = val;
}

char * sc_string(sc_val c) {
    assert(sc_stringp(c));
    return UNTAG_PTR(c, gc_string)->string;
}

char * sc_symbol_name(sc_val c) {
    assert(sc_symbolp(c));
    return UNTAG_PTR(c, gc_symbol)->name;
}

uint32_t sc_strlen(sc_val c) {
    assert(sc_stringp(c));
    return UNTAG_PTR(c, gc_string)->header.extra;
}

sc_int sc_number(sc_val n) {
    assert(sc_numberp(n));
    return UNTAG_NUMBER(n);
}

sc_val sc_make_number(sc_int n) {
    return (sc_val)TAG_NUMBER(n);
}

uint32_t sc_vector_len(sc_val v) {
    assert(sc_vectorp(v));
    return UNTAG_PTR(v, gc_vector)->header.extra;
}

sc_val sc_vector_ref(sc_val v, uint32_t n) {
    assert(sc_vectorp(v));
    assert(n < sc_vector_len(v));
    return UNTAG_PTR(v, gc_vector)->vector[n];
}

void sc_vector_set(sc_val v, uint32_t n, sc_val x) {
    assert(sc_vectorp(v));
    assert(n < sc_vector_len(v));
    UNTAG_PTR(v, gc_vector)->vector[n] = x;
}

/* Predicates */
static inline int sc_pointer_typep(sc_val c, uint32_t type) {
    return POINTERP(c)
        && !NILP(c)
        && UNTAG_PTR(c, gc_chunk)->type == type;
}

int sc_consp(sc_val c) {
    return sc_pointer_typep(c, TYPE_CONS);
}

int sc_stringp(sc_val c) {
    return sc_pointer_typep(c, TYPE_STRING);
}

int sc_symbolp(sc_val c) {
    return sc_pointer_typep(c, TYPE_SYMBOL);
}

int sc_vectorp(sc_val c) {
    return sc_pointer_typep(c, TYPE_VECTOR);
}

int sc_numberp(sc_val c) {
    return NUMBERP(c);
}

/* Memory allocation */

uintptr_t * gc_alloc(uint32_t n);

sc_val gc_alloc_cons() {
    gc_cons *cons = (gc_cons*)gc_alloc(3);
    cons->header.type = TYPE_CONS;
    cons->car = cons->cdr = NIL;
    return TAG_POINTER(cons);
}

sc_val gc_alloc_string(uint32_t len) {
    gc_string *str = (gc_string*)gc_alloc(STRLEN2CELLS(len) + 1);
    str->header.type = TYPE_STRING;
    str->header.extra = len;
    return TAG_POINTER(str);
}

sc_val gc_alloc_vector(uint32_t len) {
    gc_vector *vec = (gc_vector*)gc_alloc(MAX(len,1)+1);
    vec->header.type = TYPE_VECTOR;
    vec->header.extra = len;
    memset(vec->vector, 0, len * sizeof(sc_val));
    return TAG_POINTER(vec);
}

sc_val gc_alloc_symbol(uint32_t len) {
    /* XXX FIXME: This should be refactored better */
    gc_symbol *sym = UNTAG_PTR(gc_alloc_string(len), gc_symbol);
    sym->header.type = TYPE_SYMBOL;
    return TAG_POINTER(sym);
}

sc_val gc_make_string(char * string) {
    uint32_t len = strlen(string);
    sc_val s = gc_alloc_string(len+1);
    strcpy(sc_string(s), string);
    return s;
}

uintptr_t * gc_alloc(uint32_t n) {
    if(free_ptr - working_mem + n <= mem_size) {
        /* We have enough space in working memory */
        uintptr_t * p = free_ptr;
        free_ptr += n;
        return p;
    } else {
        /* Insufficient memory, trigger a GC */
        gc_gc();
        if(free_ptr - working_mem + n <= mem_size) {
            /* The GC gave us enough space */
            uintptr_t * p = free_ptr;
            free_ptr += n;
            return p;
        } else {
            /* This triggers another GC. If we were clever, we would
               use some heuristic to guess when we're running out of
               memory, and could gc_realloc above, instead of GCing first. */
            gc_realloc(n);
            uintptr_t * p = free_ptr;
            free_ptr += n;
            return p;
        }
    }
}

/* GC control */
void gc_init() {
    free_mem = malloc(GC_INITIAL_MEM * sizeof(uintptr_t));
    assert(!(((uintptr_t)free_mem) & 0x3));
    working_mem = malloc(GC_INITIAL_MEM * sizeof(uintptr_t));
    assert(!(((uintptr_t)working_mem) & 0x3));
    free_ptr = working_mem;
    mem_size = GC_INITIAL_MEM;

    sc_root = gc_alloc_vector(SC_NREGS);
}


sc_val gc_relocate(sc_val v) {
    int len;
    uintptr_t *reloc;
    uint32_t type;
    gc_chunk *val;

    if(NUMBERP(v) || NILP(v)) return v;
    val = UNTAG_PTR(v, gc_chunk);
    type = val->type;

    if(TYPE_BROKEN_HEART == type) {
        return val->data[0];
    }

    switch(type) {
    case TYPE_STRING:
    case TYPE_SYMBOL:
        len = (STRLEN2CELLS(val->extra) + 1);
        break;
    case TYPE_CONS:
        len = 3;
        break;
    case TYPE_VECTOR:
        len = MAX(val->extra,1) + 1;
        break;
    default:
        printf("Trying to relocate unknown data: %d!\n", type);
        return NULL;
    }

    reloc = gc_alloc(len);
    memcpy(reloc, val, sizeof(uintptr_t) * len);
    val->type = TYPE_BROKEN_HEART;
    val->data[0] = reloc;
    return reloc;
}

void gc_protect_roots() {
    int i;

    for(i=0;i<SC_NREGS;i++) {
        _sc_regs[i] = gc_relocate(_sc_regs[i]);
    }
}

void gc_gc() {
    uint32_t old_avail = gc_free_mem();
    printf("Entering garbage collection...");
    uint32_t *scan;
    uint32_t *t;
    uint32_t len;

    t = working_mem;
    working_mem = free_mem;
    scan = free_ptr = working_mem;
    free_mem = t;

    gc_protect_roots();

    while(scan != free_ptr) {
        gc_chunk *chunk = (gc_chunk*)scan;

        switch(chunk->type) {
        case TYPE_SYMBOL:
        case TYPE_STRING:
            scan += STRLEN2CELLS(chunk->extra) + 1;
            break;

        case TYPE_CONS: {
            gc_cons *cons = (gc_cons*)chunk;
            cons->car = gc_relocate(cons->car);
            cons->cdr = gc_relocate(cons->cdr);
            scan += 3;
            break;
        }

        case TYPE_VECTOR: {
            gc_vector *vec = (gc_vector*)chunk;
            uint32_t i;

            len = vec->header.extra;
            scan++;
            if(len == 0) scan++;
            for(i = 0; i < len; i++) {
                vec->vector[i] = gc_relocate(vec->vector[i]);
            }
            scan += len;
            break;
        }
        default:
            printf("GC internal error -- invalid %08x at %p!\n", *scan, scan);
            exit(-1);
        }
        if(scan > working_mem + mem_size) {
            printf("GC internal error -- ran off the end of memory!\n");
            exit(-1);
        }
    }

    /* Zero the old free memory to help catch code that accidentally
       holds onto sc_val's during GC.*/
    memset(free_mem, 0, sizeof(uintptr_t) * mem_size);
    printf("Done (freed %d words)\n", gc_free_mem() - old_avail);
}

void gc_realloc(uint32_t need) {
    printf("GC realloc forced!\n");
    uint32_t new_size = mem_size;
    while(new_size - mem_size < need) new_size <<= 1;
    printf("New memory: %d words\n", new_size);

    free(free_mem);
    free_mem = malloc(new_size * sizeof(uintptr_t));
    assert(!(((uintptr_t)free_mem) & 0x3));

    gc_gc();

    free(free_mem);
    mem_size = new_size;
    free_mem = malloc(mem_size * sizeof(uintptr_t));
    assert(!(((uintptr_t)free_mem) & 0x3));
}


uint32_t gc_free_mem() {
    return mem_size - (free_ptr - working_mem);
}

#ifdef BUILD_TEST

void test_gc(void) {
    SC_REG(0) = gc_alloc_cons();
    sc_val t = sc_make_number(32);
    sc_set_car(SC_REG(0), t);
    t = gc_alloc_cons();
    sc_set_cdr(SC_REG(0), t);
    sc_val n = gc_make_string("Hello, World\n");
    t = sc_cdr(SC_REG(0));

    sc_set_car(t, n);
    sc_set_cdr(t, NIL);

    assert(sc_numberp(sc_car(SC_REG(0))));
    assert(sc_number(sc_car(SC_REG(0))) == 32);
    assert(sc_consp(sc_cdr(SC_REG(0))));
    assert(sc_stringp(sc_car(sc_cdr(SC_REG(0)))));
    assert(!strcmp(sc_string(sc_car(sc_cdr(SC_REG(0)))),"Hello, World\n"));
    assert(NILP(sc_cdr(sc_cdr(SC_REG(0)))));

    gc_gc();

    assert(sc_numberp(sc_car(SC_REG(0))));
    assert(sc_number(sc_car(SC_REG(0))) == 32);
    assert(sc_consp(sc_cdr(SC_REG(0))));
    assert(sc_stringp(sc_car(sc_cdr(SC_REG(0)))));
    assert(!strcmp(sc_string(sc_car(sc_cdr(SC_REG(0)))),"Hello, World\n"));
    assert(NILP(sc_cdr(sc_cdr(SC_REG(0)))));

    sc_set_car(SC_REG(0), NIL);
    sc_set_cdr(SC_REG(0), NIL);
    gc_gc();


    t = gc_alloc_cons();
    sc_set_car(t, t);
    sc_set_cdr(t, SC_REG(0));
    sc_set_car(SC_REG(0), t);
    sc_set_cdr(SC_REG(0), t);

    assert(sc_car(sc_car(SC_REG(0))) == sc_car(SC_REG(0)));
    assert(sc_cdr(sc_car(SC_REG(0))) == SC_REG(0));
    assert(sc_car(SC_REG(0)) == sc_cdr(SC_REG(0)));

    gc_gc();

    assert(sc_car(sc_car(SC_REG(0))) == sc_car(SC_REG(0)));
    assert(sc_cdr(sc_car(SC_REG(0))) == SC_REG(0));
    assert(sc_car(SC_REG(0)) == sc_cdr(SC_REG(0)));

    SC_REG(0) = NIL;
    gc_gc();
    SC_REG(0) = gc_alloc_cons();

    t = gc_alloc_vector(10);
    sc_set_car(SC_REG(0), t);
    sc_set_cdr(SC_REG(0), NIL);

    int i, j;
    for(i = 0; i < 10; i++) {
        t = gc_alloc_vector(i);
        for(j = 0; j < i; j++) {
            sc_vector_set(t, j, sc_car(SC_REG(0)));
        }
        sc_vector_set(sc_car(SC_REG(0)), i, t);
    }

    assert(sc_vectorp(sc_car(SC_REG(0))));
    for(i = 0; i < 10; i++) {
        t = sc_vector_ref(sc_car(SC_REG(0)), i);
        assert(sc_vectorp(t));
        assert(sc_vector_len(t) == i);
        for(j = 0; j < i; j++) {
            assert(sc_vector_ref(t, j) == sc_car(SC_REG(0)));
        }
    }

    gc_gc();

    assert(sc_vectorp(sc_car(SC_REG(0))));
    for(i = 0; i < 10; i++) {
        t = sc_vector_ref(sc_car(SC_REG(0)), i);
        assert(sc_vectorp(t));
        assert(sc_vector_len(t) == i);
        for(j = 0; j < i; j++) {
            assert(sc_vector_ref(t, j) == sc_car(SC_REG(0)));
        }
    }

    for(i=0;i<2000;i++) {
        gc_alloc_cons();
    }

    gc_alloc_vector(0x2000);
}

#endif /* BUILD_TEST */
