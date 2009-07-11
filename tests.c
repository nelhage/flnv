#include <check.h>
#include <string.h>
#include "test/tests.h"

Suite *_construct_test_suite(struct test_suite* spec) {
    Suite *s = suite_create(spec->suite_name);
    struct test_case *c;

    for(c = spec->cases; c < spec->end_cases; c++) {
        TCase *tc = tcase_create(c->test_name);
        test_hook setup, teardown;
        test_func fn;
        for(setup = c->setup_hooks, teardown = c->teardown_hooks;
            setup < c->end_setup_hooks;
            setup++, teardown++) {
            tcase_add_checked_fixture(tc, setup, teardown);
        }
        for(fn = c->funcs; fn < c->end_funcs; fn++) {
            tcase_add_test(tc, fn);
        }
        suite_add_tcase(s, tc);
    }
    return s;
}

int main()
{
    int failed;

    SRunner *sr = srunner_create(gc_suite());
    srunner_add_suite(sr, vm_suite());

    srunner_run_all(sr, CK_ENV);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return failed != 0;
}
