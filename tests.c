#include <check.h>
#include <string.h>
#include "test/tests.h"

int main()
{
    int failed;
    Suite *s = gc_suite();

    SRunner *sr = srunner_create(s);
    srunner_add_suite(sr, vm_suite());
    srunner_run_all(sr, CK_ENV);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return failed != 0;
}
