#include "gc.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>

/* Constants */
#define GC_INITIAL_MEM  1024

#define BROKEN_HEART    ((gc_ops*)-1)

static uintptr_t *working_mem = NULL;
static uintptr_t *free_mem    = NULL;
static uintptr_t *free_ptr    = NULL;
static uintptr_t mem_size;

#ifdef TEST_STRESS_GC
static int in_gc = 0;
#endif

static gc_handle gc_root_stack = NIL;
static gc_handle gc_root_hooks = NIL;

void *_gc_try_alloc(uint32_t n) {
    if(free_ptr - working_mem + n <= mem_size) {
        void *h = free_ptr;
        free_ptr += n;
        return h;
    }
    return NIL;
}

void* _gc_alloc(uint32_t n) {
    void *handle;

#ifdef TEST_STRESS_GC
    if(!in_gc) {
        gc_gc();
    }
#endif

    handle = _gc_try_alloc(n);
    if(!handle) {
        /* Insufficient memory, trigger a GC */
        gc_gc();
        handle = _gc_try_alloc(n);
        if(!handle) {
            /* This triggers another GC. If we were clever, we would
               use some heuristic to guess when we're running out of
               memory, and could gc_realloc above, instead of GCing first. */
            gc_realloc(n);
            handle = _gc_try_alloc(n);
        }
    }
    assert(handle);
    return handle;
}

void *gc_alloc(gc_ops *ops, uint32_t n) {
    gc_chunk *handle = _gc_alloc(n);
    handle->ops = ops;
    return handle;
}

void gc_relocate_root(void);

/* GC control */
void gc_init() {
    if(free_mem) {
        free(free_mem);
        free(working_mem);
    }
    free_mem = malloc(GC_INITIAL_MEM * sizeof(uintptr_t));
    working_mem = malloc(GC_INITIAL_MEM * sizeof(uintptr_t));

    assert(!(((uintptr_t)free_mem) & 0x3));
    assert(!(((uintptr_t)working_mem) & 0x3));

    free_ptr = working_mem;
    mem_size = GC_INITIAL_MEM;

    gc_root_hooks = NIL;
    gc_root_stack = NIL;

    gc_register_gc_root_hook(gc_relocate_root);
}

/* GC internals */

void gc_relocate_nop(gc_chunk *val UNUSED) {
    /* nop */
}

typedef struct gc_external_roots {
    gc_chunk  header;
    gc_handle next_frame;
    uint32_t  nroots;
    gc_handle *roots[0];
} gc_external_roots;

static void gc_relocate_external_roots(gc_external_roots *v) {
    uint32_t i;
    gc_external_roots *r = (gc_external_roots*)v;

    gc_relocate(&r->next_frame);

    for(i = 0; i < r->nroots; i++) {
        gc_relocate(r->roots[i]);
    }
}

static uint32_t gc_len_external_roots(gc_external_roots *v) {
    return v->nroots + 2;
}

static gc_ops gc_external_root_ops = {
    .op_relocate = (gc_relocate_op)gc_relocate_external_roots,
    .op_len = (gc_len_op) gc_len_external_roots
};

void gc_register_roots(gc_handle* root0, ...) {
    int nroots = 1;
    va_list ap;
    gc_external_roots *frame;
    int i;

    va_start(ap, root0);
    while(va_arg(ap, gc_handle*)) nroots++;
    va_end(ap);

    assert(nroots <= MAX_EXTERNAL_ROOTS_FRAME);

    /*
     * Because the roots may already be live, we can't perform GC
     * until we've safely registered them. Use _try_alloc to try to
     * put them in the heap. If that fails, we stash them in a
     * statically allocated buffer, do the GC, and then stick them in
     * the heap.
     */
    frame = (gc_external_roots*)_gc_try_alloc(3 + nroots);
    assert(frame); /* XXX FIXME */

    frame->header.ops = &gc_external_root_ops;
    frame->nroots = nroots;

    i = 0;
    va_start(ap, root0);
    frame->roots[i++] = root0;
    while(--nroots)
        frame->roots[i++] = va_arg(ap, gc_handle*);
    va_end(ap);

    frame->next_frame = gc_root_stack;

    gc_root_stack = TAG_POINTER(frame);
}

void gc_pop_roots() {
    assert(!NILP(gc_root_stack));
    gc_root_stack = UNTAG_PTR(gc_root_stack, gc_external_roots)->next_frame;
}

typedef struct gc_root_hook {
    gc_chunk  header;
    gc_handle next;
    gc_hook   *hook;
} gc_root_hook;

static void gc_relocate_root_hook(gc_root_hook *hook) {
    gc_relocate(&hook->next);
}

static uint32_t gc_len_root_hook(gc_chunk *chunk) {
    return 2;
}

static struct gc_ops gc_root_hook_ops = {
    .op_relocate = (gc_relocate_op)gc_relocate_root_hook,
    .op_len      = (gc_len_op)gc_len_root_hook
};

void gc_register_gc_root_hook(gc_hook *hook_fun) {
    gc_root_hook *hook = (gc_root_hook*)gc_alloc(&gc_root_hook_ops, 3);
    hook->next = TAG_POINTER(gc_root_hooks);
    hook->hook = hook_fun;
    gc_root_hooks = TAG_POINTER(hook);
}

void gc_relocate(gc_handle *v) {
    int len;
    uintptr_t *reloc;
    gc_chunk *val;

    if(NUMBERP(*v))
        return;
    val = UNTAG_PTR(*v, gc_chunk);

    if((uintptr_t*)val < free_mem
       || (uintptr_t*)val >= (free_mem + mem_size)) {
        return;
    }

    if(val->ops == BROKEN_HEART) {
        *v = val->data[0];
        return;
    }

    assert(val->ops);

    len = val->ops->op_len(val) + 1;

    reloc = _gc_alloc(len);
    memcpy(reloc, val, sizeof(uintptr_t) * len);
    val->ops = BROKEN_HEART;
    *v = val->data[0] = reloc;
}

void gc_relocate_root() {
    gc_relocate(&gc_root_stack);
    gc_relocate(&gc_root_hooks);
}

void gc_protect_roots() {
    gc_root_hook *hook = UNTAG_PTR(gc_root_hooks, gc_root_hook);

    while(hook) {
        assert(hook->header.ops == &gc_root_hook_ops);
        hook->hook();
        hook = UNTAG_PTR(hook->next, gc_root_hook);
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

        assert(chunk->ops);

        chunk->ops->op_relocate(chunk);
        scan += chunk->ops->op_len(chunk) + 1;

        if(scan > working_mem + mem_size) {
            printf("GC internal error -- ran off the end of memory!\n");
            abort();
        }
    }

#ifdef TEST_STRESS_GC
    in_gc = 0;
#endif

#ifndef NDEBUG
    /* Zero the old free memory to help catch code that accidentally
       holds onto gc_handle's during GC.*/
    memset(free_mem, 0, sizeof(uintptr_t) * mem_size);
#endif
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
