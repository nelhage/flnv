#ifndef __MINISCHEME_GC__
#define __MINISCHEME_GC__

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define UNUSED __attribute__((unused))

struct gc_ops;
struct gc_chunk;

typedef intptr_t gc_int;
typedef uint32_t gc_handle;

typedef struct gc_chunk {
    struct gc_ops * ops;
    gc_handle data[0];
} gc_chunk;

typedef void (*gc_relocate_op)(gc_chunk*);
typedef uint32_t (*gc_len_op)(gc_chunk*);

typedef struct gc_ops {
    gc_relocate_op op_relocate;
    gc_len_op op_len;
} gc_ops;

typedef void gc_hook(void);

void gc_relocate_nop(gc_chunk *);
void gc_relocate(gc_handle *v);

/* GC control */
void gc_init();

void *gc_alloc(gc_ops *ops, uint32_t len);

void gc_realloc(uint32_t need_mem);
void gc_gc();
uint32_t gc_free_mem();

#define MAX_EXTERNAL_ROOTS_FRAME        10
void gc_register_roots(gc_handle *root0, ...);
void gc_pop_roots();

void gc_register_gc_root_hook(gc_hook *);

#define TAG_BITS     2
#define TAG_MASK     0x03

#define NUMBER_TAG   0x01
#define POINTER_TAG  0x02


/* Here be demons */
static inline gc_handle gc_tag_number(gc_int n) {
    return (n << TAG_BITS) | NUMBER_TAG;
}

static inline gc_handle gc_tag_pointer(void *p) {
    assert(!(((gc_int)p) & TAG_MASK));
    return (gc_handle)((gc_int)p) | POINTER_TAG;
}

static inline int gc_numberp(gc_handle h) {
    return (h & TAG_MASK) == NUMBER_TAG;
}

static inline int gc_pointerp(gc_handle h) {
    return (h & TAG_MASK) == POINTER_TAG;
}

static inline gc_int gc_untag_number(gc_handle h) {
    assert(gc_numberp(h));
    return ((gc_int)h >> TAG_BITS);
}

#define UNTAG_PTR(c, t) ((t*)(c & ~TAG_MASK))

#define MAX(a,b)                          \
    ({  typeof (a) _a = (a);              \
        typeof (b) _b = (b);              \
        _a > _b ? _a : _b; })

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

#define NIL          (gc_tag_pointer(NULL))
#define NILP(x)      ((x) == NIL)

#endif /* !defined(__MINISCHEME_GC__)*/
