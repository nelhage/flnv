#include <string.h>

#include "test/tests.h"

#include "gc.h"
#include "scgc.h"
#include "symbol.h"

gc_handle reg1, reg2;

static void gc_core_setup(void) {
    gc_init();
    sc_init();
    reg1 = reg2 = NIL;
    gc_register_roots(&reg1, &reg2, NULL);
}

static void gc_core_teardown(void) {
    gc_pop_roots();
}

BEGIN_SUITE(gc, "GC Test Suite");

BEGIN_TEST_CASE(gc, core, "GC Core");
SETUP_HOOK(gc, core, gc_core_setup);
TEARDOWN_HOOK(gc, core, gc_core_teardown);

TEST(gc, core, sanity_check)
{
    reg1 = sc_alloc_cons();
    sc_set_car(reg1, sc_make_number(32));
    sc_set_cdr(reg1, sc_alloc_cons());
    reg2 = sc_cdr(reg1);
    sc_set_car(reg2, sc_make_string("Hello, World\n"));
    sc_set_cdr(reg2, NIL);
    reg2 = NIL;

    fail_unless(sc_number(sc_make_number(-1234)) == -1234);
    fail_unless(sc_numberp(sc_car(reg1)));
    fail_unless(sc_number(sc_car(reg1)) == 32);
    fail_unless(sc_consp(sc_cdr(reg1)));
    fail_unless(sc_stringp(sc_car(sc_cdr(reg1))));
    fail_unless(!strcmp(sc_string_get(sc_car(sc_cdr(reg1))),"Hello, World\n"));
    fail_unless(NILP(sc_cdr(sc_cdr(reg1))));
}
END_TEST

TEST(gc, core, booleans)
{
    fail_unless(sc_booleanp(sc_true));
    fail_unless(sc_booleanp(sc_false));
    fail_unless(sc_true != sc_false);
}
END_TEST

TEST(gc, core, objs_survive_gc)
{
    reg1 = sc_alloc_cons();
    sc_set_car(reg1, sc_make_number(32));
    sc_set_cdr(reg1, sc_alloc_cons());
    reg2 = sc_cdr(reg1);
    sc_set_car(reg2, sc_make_string("Hello, World\n"));
    sc_set_cdr(reg2, NIL);
    reg2 = NIL;

    gc_gc();

    fail_unless(sc_numberp(sc_car(reg1)));
    fail_unless(sc_number(sc_car(reg1)) == 32);
    fail_unless(sc_consp(sc_cdr(reg1)));
    fail_unless(sc_stringp(sc_car(sc_cdr(reg1))));
    fail_unless(!strcmp(sc_string_get(sc_car(sc_cdr(reg1))),"Hello, World\n"));
    fail_unless(NILP(sc_cdr(sc_cdr(reg1))));
}
END_TEST

TEST(gc, core, frees_mem)
{
#ifndef TEST_STRESS_GC
    uint32_t free_mem;
    int i;
    for(i=0; i < 10; i++) {
        fail_unless((intptr_t)sc_alloc_cons());
    }
    free_mem = gc_free_mem();
    gc_gc();
    fail_unless(gc_free_mem() > free_mem);
#endif
}
END_TEST

TEST(gc, core, cons_cycle)
{
    uint32_t free_mem;
    reg1 = sc_alloc_cons();
    reg2 = sc_alloc_cons();
    sc_set_car(reg1, reg1);
    sc_set_cdr(reg1, reg2);
    sc_set_car(reg2, reg1);
    sc_set_cdr(reg2, reg1);

    fail_unless(sc_car(sc_car(reg2)) == sc_car(reg2));
    fail_unless(sc_cdr(sc_car(reg2)) == reg2);
    fail_unless(sc_car(reg2) == sc_cdr(reg2));

    gc_gc();

    fail_unless(sc_car(sc_car(reg2)) == sc_car(reg2));
    fail_unless(sc_cdr(sc_car(reg2)) == reg2);
    fail_unless(sc_car(reg2) == sc_cdr(reg2));

    gc_gc();

    fail_unless(sc_car(sc_car(reg2)) == sc_car(reg2));
    fail_unless(sc_cdr(sc_car(reg2)) == reg2);
    fail_unless(sc_car(reg2) == sc_cdr(reg2));

    reg1 = reg2 = NIL;

    free_mem = gc_free_mem();
    gc_gc();
#ifndef TEST_STRESS_GC
    fail_unless(gc_free_mem() > free_mem);
#endif
}
END_TEST

TEST(gc, core, basic_vector)
{
    int i;
    reg1 = sc_alloc_vector(10);
    fail_unless(sc_vectorp(reg1));

    for(i = 0; i < 10; i++) {
        sc_vector_set(reg1, i, sc_alloc_cons());
        sc_set_car(sc_vector_ref(reg1, i), sc_make_number(i));
    }
    gc_gc();

    fail_unless(sc_vectorp(reg1));
    for(i = 0; i < 10; i++) {
        fail_unless(sc_consp(sc_vector_ref(reg1, i)));
        fail_unless(sc_number(sc_car(sc_vector_ref(reg1, i))) == i);
    }
}
END_TEST

TEST(gc, core, large_allocs)
{
    int i;
    for(i=0;i<2000;i++) {
        sc_alloc_cons();
    }
    sc_alloc_vector(0x2000);
    reg1 = sc_alloc_cons();

    gc_gc();

    fail_unless(sc_consp(reg1));
}
END_TEST

TEST(gc, core, many_allocs)
{
    int i;
    reg2 = reg1 = sc_alloc_cons();
    for(i = 0; i < 2000; i++) {
        sc_set_car(reg2, sc_make_number(i));
        sc_set_cdr(reg2, sc_alloc_cons());
        reg2 = sc_cdr(reg2);
    }

    gc_gc();

    reg2 = reg1;
    for(i = 0; i < 2000; i++) {
        fail_unless(sc_consp(reg2));
        fail_unless(sc_numberp(sc_car(reg2)));
        fail_unless(sc_number(sc_car(reg2)) == i);
        reg2 = sc_cdr(reg2);
    }
}
END_TEST

gc_handle external_root;
void gc_reloc_external() {
    gc_relocate(&external_root);
}

TEST(gc, core, root_hook)
{
    gc_register_gc_root_hook(gc_reloc_external);
    external_root = sc_alloc_cons();
    sc_set_car(external_root, external_root);
    sc_set_cdr(external_root, NIL);

    gc_gc();

    fail_unless(sc_consp(external_root));
    fail_unless(sc_consp(sc_car(external_root)));
    fail_unless(external_root == sc_car(external_root));
}
END_TEST

TEST(gc, core, roots)
{
    gc_handle reg;

    gc_register_roots(&reg, NULL);

    reg = reg1 = sc_alloc_cons();

    gc_gc();

    fail_unless(sc_consp(reg1));
    fail_unless(reg == reg1);

    gc_pop_roots();

    gc_gc();

    fail_unless(reg != reg1);
}
END_TEST

TEST(gc, core, live_roots)
{
    gc_handle reg;
    reg = reg1 = sc_alloc_cons();

    /* Use up all available memory */
    gc_alloc(NULL, gc_free_mem());

    gc_register_roots(&reg, NULL);

    gc_gc();

    fail_unless(sc_consp(reg1));
    fail_unless(reg == reg1);

    gc_pop_roots();
}
END_TEST

END_TEST_CASE(gc, core);

static void obarray_setup() {
    obarray_init();
}

static void obarray_teardown() {

}

BEGIN_TEST_CASE(gc, obarray, "obarray");
SETUP_HOOK(gc, obarray, gc_core_setup);
SETUP_HOOK(gc, obarray, obarray_setup);
TEARDOWN_HOOK(gc, obarray, gc_core_teardown);
TEARDOWN_HOOK(gc, obarray, obarray_teardown);

TEST(gc, obarray, sancheck)
{
    reg1 = sc_intern_symbol("hello");
    fail_unless(sc_symbolp(reg1));
    fail_unless(!strcmp(sc_symbol_name(reg1), "hello"));
    reg2 = sc_intern_symbol("hello");
    fail_unless(reg1 == reg2);
}
END_TEST

TEST(gc, obarray, realloc)
{
    int i;
    char str[2] = "a";
    reg1 = sc_intern_symbol("hello");
    /* Intern a lot of junk to force an obarray realloc */
    for(i = 0; i < 26; i++) {
        str[0] = 'a' + i;
        fail_unless(sc_symbolp(sc_intern_symbol(str)));
        str[0] = 'A' + i;
        fail_unless(sc_symbolp(sc_intern_symbol(str)));
    }

    fail_unless(!strcmp(sc_symbol_name(reg1), "hello"));
    fail_unless(sc_intern_symbol("hello") == reg1);
}
END_TEST

END_TEST_CASE(gc, obarray);
END_SUITE(gc);

Suite *gc_suite()
{
    return construct_test_suite(gc);
}
