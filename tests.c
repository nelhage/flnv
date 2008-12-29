#include <check.h>
#include <string.h>

#include "gc.h"
#include "scgc.h"
#include "symbol.h"

gc_handle reg1, reg2;

static void gc_core_setup(void) {
    gc_init();
    gc_register_roots(&reg1, &reg2, NULL);
}

static void gc_core_teardown(void) {
    gc_pop_roots();
}

START_TEST(gc_sanity_check)
{
    reg1 = sc_alloc_cons();
    sc_set_car(reg1, sc_make_number(32));
    sc_set_cdr(reg1, sc_alloc_cons());
    reg2 = sc_cdr(reg1);
    sc_set_car(reg2, sc_make_string("Hello, World\n"));
    sc_set_cdr(reg2, NIL);
    reg2 = NIL;

    fail_unless(sc_numberp(sc_car(reg1)));
    fail_unless(sc_number(sc_car(reg1)) == 32);
    fail_unless(sc_consp(sc_cdr(reg1)));
    fail_unless(sc_stringp(sc_car(sc_cdr(reg1))));
    fail_unless(!strcmp(sc_string_get(sc_car(sc_cdr(reg1))),"Hello, World\n"));
    fail_unless(NILP(sc_cdr(sc_cdr(reg1))));
}
END_TEST

START_TEST(objs_survive_gc)
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

START_TEST(gc_frees_mem)
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

START_TEST(gc_cons_cycle)
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

START_TEST(gc_basic_vector)
{
    int i;
    reg1 = sc_alloc_vector(10);
    for(i = 0; i < 10; i++) {
        sc_vector_set(reg1, i, sc_alloc_cons());
        sc_set_car(sc_vector_ref(reg1, i), sc_make_number(i));
    }
    gc_gc();
    for(i = 0; i < 10; i++) {
        fail_unless(sc_consp(sc_vector_ref(reg1, i)));
        fail_unless(sc_number(sc_car(sc_vector_ref(reg1, i))) == i);
    }
}
END_TEST

START_TEST(gc_large_allocs)
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

gc_handle external_root;
void gc_reloc_external() {
    gc_relocate(&external_root);
}

START_TEST(gc_root_hook)
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

START_TEST(gc_roots)
{
    gc_handle reg;

    reg = reg1 = sc_alloc_cons();

    gc_register_roots(&reg, NULL);

    gc_gc();

    fail_unless(reg == reg1);

    gc_pop_roots();

    gc_gc();

    fail_unless(reg != reg1);
}
END_TEST

static void obarray_setup() {
    obarray_init();
}

static void obarray_teardown() {

}

START_TEST(obarray_sancheck)
{
    reg1 = sc_intern_symbol("hello");
    fail_unless(sc_symbolp(reg1));
    fail_unless(!strcmp(sc_symbol_name(reg1), "hello"));
    reg2 = sc_intern_symbol("hello");
    fail_unless(reg1 == reg2);
}
END_TEST

START_TEST(obarray_realloc)
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


Suite *gc_suite()
{
    Suite *s = suite_create("GC Test Suites");
    TCase *tc_core = tcase_create("GC core");
    tcase_add_checked_fixture(tc_core,
                              gc_core_setup,
                              gc_core_teardown);
    tcase_add_test(tc_core, gc_sanity_check);
    tcase_add_test(tc_core, objs_survive_gc);
    tcase_add_test(tc_core, gc_frees_mem);
    tcase_add_test(tc_core, gc_cons_cycle);
    tcase_add_test(tc_core, gc_basic_vector);
    tcase_add_test(tc_core, gc_large_allocs);
    tcase_add_test(tc_core, gc_root_hook);
    tcase_add_test(tc_core, gc_roots);
    suite_add_tcase(s, tc_core);

    TCase *tc_obarray = tcase_create("obarray");

    tcase_add_checked_fixture(tc_obarray,
                              gc_core_setup,
                              gc_core_teardown);
    tcase_add_checked_fixture(tc_obarray,
                              obarray_setup,
                              obarray_teardown);
    tcase_add_test(tc_obarray, obarray_sancheck);
    tcase_add_test(tc_obarray, obarray_realloc);
    suite_add_tcase(s, tc_obarray);

    return s;
}

int main()
{
    int failed;
    Suite *s = gc_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_ENV);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return failed != 0;
}
