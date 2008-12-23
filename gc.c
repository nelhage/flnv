#include "gc.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>

/* Constants */
#define GC_INITIAL_MEM  1024

static uintptr_t * working_mem;
static uintptr_t * free_mem;
static uintptr_t * free_ptr;
static uintptr_t mem_size;

#ifdef TEST_STRESS_GC
static int in_gc = 0;
#endif

static sc_val sc_root_stack = NIL;
static sc_val gc_root_hooks = NIL;

#define TAG_NUMBER(x)  ((sc_val)((x)<<1 | 0x1))
#define TAG_POINTER(x) ((sc_val)(x))

#define NUMBERP(x) ((uintptr_t)(x)&0x1)
#define POINTERP(x)  (!NUMBERP(x))

#define UNTAG_PTR(c, t) ((t*)c)
#define UNTAG_NUMBER(c) (((sc_int)(c))>>1)

#define MAX(a,b)                          \
    ({  typeof (a) _a = (a);              \
        typeof (b) _b = (b);              \
        _a > _b ? _a : _b; })

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

enum {
    TYPE_SYMBOL,
    TYPE_STRING,
    TYPE_CONS,
    TYPE_VECTOR,
    TYPE_EXTERNAL_ROOTS,
    TYPE_MAX,
    TYPE_BROKEN_HEART = (uint8_t)-1
};

/* Public interfaces */

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
    char name[];
} gc_symbol;

typedef struct gc_string {
    gc_chunk header;
    char string[];
} gc_string;

typedef struct gc_vector {
    gc_chunk header;
    sc_val   vector[];
} gc_vector;

typedef struct gc_external_roots {
    gc_chunk header;
    sc_val   next_frame;
    sc_val   *roots[];
} gc_external_roots;

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

#define MAX_FRAME_SIZE ROUNDUP(sizeof(gc_external_roots) +               \
                               MAX_EXTERNAL_ROOTS_FRAME*sizeof(sc_val*), \
                               sizeof(sc_val)) /                         \
    sizeof(sc_val)

uintptr_t * gc_alloc(uint32_t n) {
    if(free_ptr - working_mem + n + MAX_FRAME_SIZE <= mem_size) {
#ifdef TEST_STRESS_GC
        if(!in_gc) {
            gc_gc();
        }
#endif
        /* We have enough space in working memory */
        uintptr_t * p = free_ptr;
        free_ptr += n;
        return p;
    } else {
        /* Insufficient memory, trigger a GC */
        gc_gc();
        if(free_ptr - working_mem + n + MAX_FRAME_SIZE <= mem_size) {
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

    gc_register_roots(&gc_root_hooks, NULL);
}

/* GC internals */

typedef struct gc_ops {
    void (*op_relocate)(gc_chunk *val);
    uint32_t (*op_len)(gc_chunk *val);
} gc_ops;

void gc_relocate_nop(gc_chunk *val UNUSED) {
    /* nop */
}

void gc_relocate_cons(gc_chunk *v) {
    gc_cons *cons = (gc_cons*)v;
    gc_relocate(&cons->car);
    gc_relocate(&cons->cdr);
}

void gc_relocate_vector(gc_chunk *v) {
    uint32_t i;
    gc_vector *vec = (gc_vector*)v;

    for(i = 0; i < vec->header.extra; i++) {
        gc_relocate(&vec->vector[i]);
    }
}

void gc_relocate_external_roots(gc_chunk *v) {
    uint32_t i;
    gc_external_roots *r = (gc_external_roots*)v;

    gc_relocate(&r->next_frame);

    for(i = 0; i < r->header.extra; i++) {
        gc_relocate(r->roots[i]);
    }
}

uint32_t gc_len_string(gc_chunk *v) {
    return STRLEN2CELLS(v->extra);
}

uint32_t gc_len_cons(gc_chunk *v UNUSED) {
    return 2;
}

uint32_t gc_len_vector(gc_chunk *v) {
    return MAX((uint32_t)v->extra, 1);
}

uint32_t gc_len_external_roots(gc_chunk *v) {
    return v->extra + 1;
}

gc_ops gc_op_table[TYPE_MAX] = {
    [TYPE_SYMBOL] {gc_relocate_nop, gc_len_string},
    [TYPE_STRING] {gc_relocate_nop, gc_len_string},
    [TYPE_CONS]   {gc_relocate_cons, gc_len_cons},
    [TYPE_VECTOR] {gc_relocate_vector, gc_len_vector},
    [TYPE_EXTERNAL_ROOTS] {gc_relocate_external_roots, gc_len_external_roots}
};

void gc_register_roots(sc_val* root0, ...) {
    int nroots = 1;
    va_list ap;
    gc_external_roots *frame;
    int i = 0;

    va_start(ap, root0);
    while(va_arg(ap, sc_val*)) nroots++;
    va_end(ap);

    assert(nroots <= MAX_EXTERNAL_ROOTS_FRAME);

    /*
     * Because the roots may already be live, we can't perform GC
     * until we've safely registered them. gc_alloc will ensure we
     * always have space to allocate new roots, and we set in_gc to
     * true to force gc_alloc not to do a gc_gc() even if we're in
     * stress-testing mode.
     */
    assert(gc_free_mem() >= sizeof *frame + nroots * sizeof(sc_val*));
#ifdef TEST_STRESS_GC
    in_gc = 1;
#endif
    frame = (gc_external_roots*)gc_alloc(sizeof *frame +
                                         nroots * sizeof(sc_val*));
#ifdef TEST_STRESS_GC
    in_gc = 0;
#endif

    frame->header.type = TYPE_EXTERNAL_ROOTS;
    frame->header.extra = nroots;

    va_start(ap, root0);
    frame->roots[i++] = root0;
    while(--nroots)
        frame->roots[i++] = va_arg(ap, sc_val*);
    va_end(ap);

    frame->next_frame = sc_root_stack;

    sc_root_stack = TAG_POINTER(frame);
}

void gc_pop_roots() {
    assert(!NILP(sc_root_stack));
    sc_root_stack = UNTAG_PTR(sc_root_stack, gc_external_roots)->next_frame;
}

void gc_register_gc_root_hook(gc_hook *hook) {
    sc_val pair = gc_alloc_cons();
    sc_set_cdr(pair, gc_root_hooks);
    sc_set_car(pair, TAG_POINTER(hook));
    gc_root_hooks = pair;
}

void gc_relocate(sc_val *v) {
    int len;
    uintptr_t *reloc;
    int type;
    gc_chunk *val;

    if(NUMBERP(*v))
        return;
    val = UNTAG_PTR(*v, gc_chunk);

    if((uintptr_t*)val < free_mem
       || (uintptr_t*)val >= (free_mem + mem_size)) {
        return;
    }
    type = val->type;

    if(TYPE_BROKEN_HEART == type) {
        *v = val->data[0];
        return;
    }

    assert(type < TYPE_MAX);

    len = gc_op_table[type].op_len(val) + 1;

    reloc = gc_alloc(len);
    memcpy(reloc, val, sizeof(uintptr_t) * len);
    val->type = TYPE_BROKEN_HEART;
    *v = val->data[0] = reloc;
}

void gc_protect_roots() {
    sc_val hook = gc_root_hooks;
    gc_relocate(&sc_root_stack);
    while(!NILP(hook)) {
        UNTAG_PTR(sc_car(hook), gc_hook)();
        hook = sc_cdr(hook);
    }
}

void gc_gc() {
    uint32_t old_avail = gc_free_mem();
    printf("Entering garbage collection...");
    uint32_t *scan;
    uint32_t *t;

#ifdef TEST_STRESS_GC
    if(in_gc) {
        printf("GC internal error -- recursive GC!\n");
        abort();
    }
    in_gc = 1;
#endif

    t = working_mem;
    working_mem = free_mem;
    free_mem = t;

    scan = free_ptr = working_mem;

    gc_protect_roots();

    while(scan != free_ptr) {
        gc_chunk *chunk = (gc_chunk*)scan;

        assert(chunk->type < TYPE_MAX);

        gc_op_table[chunk->type].op_relocate(chunk);
        scan += gc_op_table[chunk->type].op_len(chunk) + 1;

        if(scan > working_mem + mem_size) {
            printf("GC internal error -- ran off the end of memory!\n");
            abort();
        }
    }

#ifdef TEST_STRESS_GC
    in_gc = 0;
#endif

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

static int gc_count = 0;

static sc_val external_root = NIL;

void gc_reloc_external() {
    gc_count++;
    gc_relocate(&external_root);
}

void test_gc(void) {
    int c;
    uint32_t free_mem;
    sc_val reg = NIL, reg2 = NIL;
    sc_val t = NIL;
    gc_register_roots(&reg, &t, NULL);
    gc_register_gc_root_hook(gc_reloc_external);

    reg = gc_alloc_cons();
    sc_set_car(reg, sc_make_number(32));
    sc_set_cdr(reg, gc_alloc_cons());
    t = sc_cdr(reg);
    sc_set_car(t, gc_make_string("Hello, World\n"));
    sc_set_cdr(t, NIL);

    assert(sc_numberp(sc_car(reg)));
    assert(sc_number(sc_car(reg)) == 32);
    assert(sc_consp(sc_cdr(reg)));
    assert(sc_stringp(sc_car(sc_cdr(reg))));
    assert(!strcmp(sc_string(sc_car(sc_cdr(reg))),"Hello, World\n"));
    assert(NILP(sc_cdr(sc_cdr(reg))));

    c = gc_count;
    gc_gc();
    assert(c + 1 == gc_count);

    assert(sc_numberp(sc_car(reg)));
    assert(sc_number(sc_car(reg)) == 32);
    assert(sc_consp(sc_cdr(reg)));
    assert(sc_stringp(sc_car(sc_cdr(reg))));
    assert(!strcmp(sc_string(sc_car(sc_cdr(reg))),"Hello, World\n"));
    assert(NILP(sc_cdr(sc_cdr(reg))));

    sc_set_car(reg, NIL);
    sc_set_cdr(reg, NIL);

    free_mem = gc_free_mem();
    c = gc_count;
    gc_gc();
    assert(c + 1 == gc_count);
#ifndef TEST_STRESS_GC
    assert(gc_free_mem() > free_mem);
#endif

    t = gc_alloc_cons();
    sc_set_car(t, t);
    sc_set_cdr(t, reg);
    sc_set_car(reg, t);
    sc_set_cdr(reg, t);

    assert(sc_car(sc_car(reg)) == sc_car(reg));
    assert(sc_cdr(sc_car(reg)) == reg);
    assert(sc_car(reg) == sc_cdr(reg));

    c = gc_count;
    gc_gc();
    assert(c + 1 == gc_count);

    assert(sc_car(sc_car(reg)) == sc_car(reg));
    assert(sc_cdr(sc_car(reg)) == reg);
    assert(sc_car(reg) == sc_cdr(reg));

    reg = NIL;

    free_mem = gc_free_mem();
    c = gc_count;
    gc_gc();
    assert(c + 1 == gc_count);
#ifndef TEST_STRESS_GC
    assert(gc_free_mem() > free_mem);
#endif

    reg = gc_alloc_cons();

    sc_set_car(reg, gc_alloc_vector(10));
    sc_set_cdr(reg, NIL);

    int i, j;
    for(i = 0; i < 10; i++) {
        t = gc_alloc_vector(i);
        for(j = 0; j < i; j++) {
            sc_vector_set(t, j, sc_car(reg));
        }
        sc_vector_set(sc_car(reg), i, t);
    }

    assert(sc_vectorp(sc_car(reg)));
    for(i = 0; i < 10; i++) {
        t = sc_vector_ref(sc_car(reg), i);
        assert(sc_vectorp(t));
        assert(sc_vector_len(t) == i);
        for(j = 0; j < i; j++) {
            assert(sc_vector_ref(t, j) == sc_car(reg));
        }
    }

    gc_gc();

    assert(sc_vectorp(sc_car(reg)));
    for(i = 0; i < 10; i++) {
        t = sc_vector_ref(sc_car(reg), i);
        assert(sc_vectorp(t));
        assert(sc_vector_len(t) == i);
        for(j = 0; j < i; j++) {
            assert(sc_vector_ref(t, j) == sc_car(reg));
        }
    }

    external_root = gc_alloc_cons();
    sc_set_car(external_root, external_root);
    sc_set_cdr(external_root, NIL);

    gc_gc();

    assert(sc_consp(external_root));
    assert(sc_consp(sc_car(external_root)));
    assert(external_root == sc_car(external_root));

    for(i=0;i<2000;i++) {
        gc_alloc_cons();
    }

    gc_alloc_vector(0x2000);

    reg = gc_alloc_cons();
    sc_set_car(reg, NIL);
    sc_set_cdr(reg, NIL);

    reg2 = reg;

    gc_register_roots(&reg2);
    gc_gc();
    assert(reg == reg2);
    gc_pop_roots();
    gc_gc();
    assert(reg != reg2);

    gc_pop_roots();
}

#endif /* BUILD_TEST */
