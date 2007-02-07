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

static uint32_t * working_mem;
static uint32_t * free_mem;
static uint32_t * free_ptr;
static uint32_t mem_size;

#define TAG_NUMBER(x)  ((x)<<1 | 0x1)
#define TAG_POINTER(x) (x)

#define NUMBERP(x) ((uint32_t)(x)&0x1)
#define POINTERP(x)  (!NUMBERP(x))

#define UNTAG_PTR(c) (c)
#define UNTAG_NUMBER(c) (((uint32_t)(c))>>1)

#define MAX(a,b) (((a)>(b))?(a):(b))


#define TYPE_SYMBOL 1
#define TYPE_STRING 2
#define TYPE_CONS   3
#define TYPE_VECTOR 4

#define TYPE_BITS   3

#define TYPE(x) (((uint32_t)(x)) & ((1 << TYPE_BITS) - 1))
#define ADD_TYPE(x,t) ((sc_val)(((uint32_t)(x) << TYPE_BITS) | t))
#define REMOVE_TYPE(x) ((uint32_t)(x) >> TYPE_BITS)

#define BROKEN_HEART ((uint32_t)0xffffffff)

sc_val sc_car(sc_val c) {
    assert(sc_consp(c));
    return (sc_val)(*(UNTAG_PTR(c)+1));
}

sc_val sc_cdr(sc_val c) {
    assert(sc_consp(c));
    return (sc_val)(*(UNTAG_PTR(c)+2));
}

void sc_set_car(sc_val c, sc_val val) {
    assert(sc_consp(c));
    *(UNTAG_PTR(c)+1) = (uint32_t)(val);
}

void sc_set_cdr(sc_val c, sc_val val) {
    assert(sc_consp(c));
    *(UNTAG_PTR(c)+2) = (uint32_t)(val);
}

char * sc_string(sc_val c) {
    assert(sc_stringp(c));
    return (char*)(UNTAG_PTR(c) + 1);
}

char * sc_symbol_name(sc_val c) {
    assert(sc_symbolp(c));
    return (char*)(UNTAG_PTR(c) + 1);
}

uint32_t sc_strlen(sc_val c) {
    assert(sc_stringp(c));
    return REMOVE_TYPE(*(UNTAG_PTR(c)));
}

int sc_number(sc_val n) {
    assert(sc_numberp(n));
    return UNTAG_NUMBER(n);
}

sc_val sc_make_number(int n) {
    return (sc_val)TAG_NUMBER(n);
}

uint32_t sc_vector_len(sc_val v) {
    assert(sc_vectorp(v));
    return REMOVE_TYPE(*UNTAG_PTR(v));
}

sc_val sc_vector_ref(sc_val v, uint32_t n) {
    assert(sc_vectorp(v));
    assert(n < sc_vector_len(v));
    return (sc_val)*(UNTAG_PTR(v) + n + 1);
}

void sc_vector_set(sc_val v, uint32_t n, sc_val x) {
    assert(sc_vectorp(v));
    assert(n < sc_vector_len(v));
    *(UNTAG_PTR(v) + n + 1) = (uint32_t)x;
}

/* Predicates */
static inline int sc_pointer_typep(sc_val c, uint32_t type) {
    return POINTERP(c)
        && !NILP(c)
        && TYPE(*UNTAG_PTR(c)) == type;
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

uint32_t * gc_alloc(uint32_t n);

sc_val gc_alloc_cons() {
    uint32_t * p = gc_alloc(3);
    *p = TYPE_CONS;
    *(p + 1) = *(p + 2) = (uint32_t)NIL;
    return TAG_POINTER(p);
}

sc_val gc_alloc_string(uint32_t len) {
    uint32_t * p = gc_alloc((len+3)/4 + 1);
    *p = (uint32_t)ADD_TYPE(len, TYPE_STRING);
    return TAG_POINTER(p);
}

sc_val gc_alloc_vector(uint32_t len) {
    uint32_t * p = gc_alloc(MAX(len,1)+1);
    *p = (uint32_t)ADD_TYPE(len, TYPE_VECTOR);
    memset(p+1, 0, len * sizeof(sc_val));
    return TAG_POINTER(p);
}

sc_val gc_alloc_symbol(uint32_t len) {
    /* XXX FIXME: This should be refactored better */
    sc_val v = gc_alloc_string(len);
    *(UNTAG_PTR(v)) = (uint32_t)ADD_TYPE(REMOVE_TYPE(*(UNTAG_PTR(v))), TYPE_SYMBOL);
    return v;
}

sc_val gc_make_string(char * string) {
    uint32_t len = strlen(string);
    sc_val s = gc_alloc_string(len+1);
    strcpy(sc_string(s), string);
    return s;
}

uint32_t * gc_alloc(uint32_t n) {
    if(free_ptr - working_mem + n <= mem_size) {
        /* We have enough space in working memory */
        uint32_t * p = free_ptr;
        free_ptr += n;
        return p;
    } else {
        /* Insufficient memory, trigger a GC */
        gc_gc();
        if(free_ptr - working_mem + n <= mem_size) {
            /* The GC gave us enough space */
            uint32_t * p = free_ptr;
            free_ptr += n;
            return p;
        } else {
            /* This triggers another GC. If we were clever, we would
               use some heuristic to guess when we're running out of
               memory, and could gc_realloc above, instead of GCing first. */
            gc_realloc(n);
            uint32_t * p = free_ptr;
            free_ptr += n;
            return p;
        }
    }
}

/* GC control */
void gc_init() {
    free_mem = malloc(GC_INITIAL_MEM * sizeof(uint32_t));
    assert(!(((uint32_t)free_mem) & 0x3));
    working_mem = malloc(GC_INITIAL_MEM * sizeof(uint32_t));
    assert(!(((uint32_t)working_mem) & 0x3));
    free_ptr = working_mem;
    mem_size = GC_INITIAL_MEM;

    sc_root = gc_alloc_vector(SC_NREGS);
}


sc_val gc_relocate(sc_val v) {
    int len;
    uint32_t * ptr, *reloc;
    uint32_t type;
    if(NUMBERP(v) || NILP(v)) return v;
    ptr = UNTAG_PTR(v);
    type = TYPE(*ptr);

    if(BROKEN_HEART == *ptr) {
        return (sc_val)*(ptr + 1);
    }

    switch(type) {
    case TYPE_STRING:
    case TYPE_SYMBOL:
        len = ((REMOVE_TYPE(*ptr)+3)/4 + 1);
        break;
    case TYPE_CONS:
        len = 3;
        break;
    case TYPE_VECTOR:
        len = MAX(REMOVE_TYPE(*ptr),1) + 1;
        break;
    default:
        printf("Trying to relocate unknown data: %d!\n", type);
        return NULL;
    }

    reloc = gc_alloc(len);
    memcpy(reloc, ptr, sizeof(uint32_t) * len);
    *ptr = BROKEN_HEART;
    *(ptr + 1) = (uint32_t)(reloc);
    return (sc_val)*(ptr + 1);
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
    uint32_t *scan, *t;
    uint32_t len;

    t = working_mem;
    working_mem = free_mem;
    scan = free_ptr = working_mem;
    free_mem = t;

    gc_protect_roots();

    while(scan != free_ptr) {
        switch(TYPE(*scan)) {
        case TYPE_SYMBOL:
        case TYPE_STRING:
            scan += (REMOVE_TYPE(*scan)+3)/4 + 1;
            break;
        case TYPE_CONS:
            *(scan+1) = (uint32_t)gc_relocate((sc_val)*(scan+1));
            *(scan+2) = (uint32_t)gc_relocate((sc_val)*(scan+2));
            scan += 3;
            break;
        case TYPE_VECTOR:
            len = REMOVE_TYPE(*scan);
            scan++;
            if(len == 0) scan++;
            while(len--) {
                *scan = (uint32_t)gc_relocate((sc_val)*scan);
                scan++;
            }
            break;
        default:
            printf("GC internal error -- invalid 0x%08x at 0x%08x!\n", *scan, (uint32_t)scan);
            exit(-1);
        }
        if(scan > working_mem + mem_size) {
            printf("GC internal error -- ran off the end of memory!\n");
            exit(-1);
        }
    }

    /* Zero the old free memory to help catch code that accidentally
       holds onto sc_val's during GC.*/
    memset(free_mem, 0, sizeof(uint32_t) * mem_size);
    printf("Done (freed %d words)\n", gc_free_mem() - old_avail);
}

void gc_realloc(uint32_t need) {
    printf("GC realloc forced!\n");
    uint32_t new_size = mem_size;
    while(new_size - mem_size < need) new_size <<= 1;
    printf("New memory: %d words\n", new_size);

    free(free_mem);
    free_mem = malloc(new_size * sizeof(uint32_t));
    assert(!(((uint32_t)free_mem) & 0x3));

    gc_gc();

    free(free_mem);
    mem_size = new_size;
    free_mem = malloc(mem_size * sizeof(uint32_t));
    assert(!(((uint32_t)free_mem) & 0x3));
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
