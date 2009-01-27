#include "test/tests.h"

#include "scgc.h"
#include "vm.h"
#include "vm_ops.h"

static void vm_setup() {
    gc_init();
    sc_init();
    vm_init();
}

static void vm_teardown() {
    
}

#define EMIT_OP(op) do {                        \
        *(code++) = op;                         \
    } while(0)

#define EMIT_INT(i) do {                        \
        *((int*)code) = i;                      \
        code += sizeof(int);                    \
    } while(0);

START_TEST(vm_load_int)
{
    uint8_t buf[1024];
    uint8_t *code = buf;
    EMIT_OP(OP_PUSH_INT);
    EMIT_INT(0);

    vm_set_ip(buf);
    vm_step_one();

    gc_handle h = vm_pop();
    fail_unless(sc_numberp(h));
    fail_unless(0 == sc_number(h));

    code = buf;
    EMIT_OP(OP_PUSH_INT);
    EMIT_INT(100);

    EMIT_OP(OP_PUSH_INT);
    EMIT_INT(200);

    vm_set_ip(buf);
    vm_step_one();
    vm_step_one();

    h = vm_pop();
    fail_unless(sc_numberp(h));
    fail_unless(200 == sc_number(h));

    h = vm_pop();
    fail_unless(sc_numberp(h));
    fail_unless(100 == sc_number(h));
}
END_TEST

START_TEST(vm_arithmetic)
{
    uint8_t buf[1024];
    uint8_t *code = buf;
    int ops = 0;
    gc_handle v;

    vm_set_ip(buf);

    EMIT_OP(OP_PUSH_INT);
    EMIT_INT(1024);
    vm_step_one();

    EMIT_OP(OP_PUSH_INT); ops++;
    EMIT_INT(72);
    vm_step_one();

    EMIT_OP(OP_ADD); ops++;
    vm_step_one();

    v = vm_top();
    fail_unless(sc_numberp(v));
    fail_unless(sc_number(v) == (1024+72));

    EMIT_OP(OP_PUSH_INT); ops++;
    EMIT_INT(7777);
    vm_step_one();

    EMIT_OP(OP_SUB); ops++;
    vm_step_one();

    v = vm_top();
    fail_unless(sc_numberp(v));
    fail_unless(sc_number(v) == (1024+72)-7777);

    EMIT_OP(OP_PUSH_INT); ops++;
    EMIT_INT(1234);
    vm_step_one();

    EMIT_OP(OP_MUL); ops++;
    vm_step_one();

    v = vm_top();
    fail_unless(sc_numberp(v));
    fail_unless(sc_number(v) == ((1024+72)-7777)*1234);
}
END_TEST

Suite *vm_suite()
{
    Suite *s = suite_create("VM Test Suites");
    TCase *tc = tcase_create("VM tests");

    tcase_add_checked_fixture(tc, vm_setup, vm_teardown);

    tcase_add_test(tc, vm_load_int);
    tcase_add_test(tc, vm_arithmetic);

    suite_add_tcase(s, tc);

    return s;
}
