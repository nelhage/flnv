#include <check.h>

#define __used __attribute__((used))

Suite *gc_suite();
Suite *vm_suite();

typedef void (*test_func)(int);
typedef void (*test_hook)(void);

struct test_case {
    test_func  *funcs, *end_funcs;
    const char *test_name;
    test_hook *setup_hooks, *end_setup_hooks;
    test_hook *teardown_hooks, *end_teardown_hooks;
};

struct test_suite {
    struct test_case *cases, *end_cases;
    const char *suite_name;
};

#define BEGIN_SUITE(sym, name)                                          \
    extern struct test_case _begin_ ## sym ## _cases[];                 \
    extern struct test_case _end_ ## sym ## _cases[];                   \
    struct test_suite _suite_ ## sym = {                                \
        .cases       = (struct test_case*)_begin_ ## sym ## _cases,     \
        .end_cases   = (struct test_case*)_end_ ## sym ## _cases,       \
        .suite_name  = name,                                            \
    };                                                                  \
    struct test_case _begin_ ## sym ## _cases[0]                        \
    __attribute__((section(".data." #sym ".cases")));                   \

#define BEGIN_TEST_CASE(suite, test, name)                      \
    _BEGIN_TEST_CASE(suite, suite ## _ ## test,                 \
                ".data." #suite "." #test, name)

#define _BEGIN_TEST_CASE(suite, sym, sec, name)                 \
    extern test_func _begin_ ## sym ## _funcs[];                \
    extern test_hook _begin_ ## sym ## _setup[];                \
    extern test_hook _begin_ ## sym ## _teardown[];             \
    extern test_func _end_ ## sym ## _funcs[];                  \
    extern test_hook _end_ ## sym ## _setup[];                  \
    extern test_hook _end_ ## sym ## _teardown[];               \
    struct test_case _test_ ## sym                              \
    __attribute__((section(".data." #suite ".cases"))) = {      \
        .test_name = name,                                      \
        .funcs = _begin_ ## sym ## _funcs,                      \
        .end_funcs = _end_ ## sym ## _funcs,                    \
        .setup_hooks = _begin_ ## sym ## _setup,                \
        .end_setup_hooks = _end_ ## sym ## _setup,              \
        .teardown_hooks  = _begin_ ## sym ## _teardown,         \
        .end_teardown_hooks  = _end_ ## sym ## _teardown,       \
    };                                                          \
    test_func _begin_ ## sym ## _funcs[0]                       \
    __attribute__((section(sec ".funcs")));                     \
    test_hook _begin_ ## sym ## _setup[0]                       \
    __attribute__((section(sec ".setup")));                     \
    test_hook _begin_ ## sym ## _teardown[0]                    \
    __attribute__((section(sec ".teardown")))                   \

#define SETUP_HOOK(suite, test, hook)                                   \
    test_hook *_setup_##suite##_##test##_##hook __used                  \
    __attribute__((section(".data." #suite "." #test ".setup"))) =      \
         hook;

#define TEARDOWN_HOOK(suite, test, hook)                                \
    test_hook *_teardown_##suite##_##test##_##hook __used               \
    __attribute__((section(".data." #suite "." #test ".teardown"))) =   \
         hook;

#define TEST(suite, test, fn)                           \
    _TEST(suite ## _ ## test,                           \
          ".data." #suite "." #test,                    \
          suite##_##test##_##fn)

#define _TEST(sym, sec, fn)                                     \
    static void fn(int);                                        \
    test_func *_test_##fn __used                                \
    __attribute__((section(sec ".funcs"))) = fn;                \
    START_TEST(fn)

#define END_TEST_CASE(suite, test)                      \
    _END_TEST_CASE(suite ## _ ## test,                  \
                   ".data." #suite "." #test)           \

#define _END_TEST_CASE(sym, sec)                        \
    test_func _end_ ## sym ## _funcs[0]                 \
    __attribute__((section(sec ".funcs")));             \
    test_hook _end_ ## sym ## _setup[0]                 \
    __attribute__((section(sec ".setup")));             \
    test_hook _end_ ## sym ## _teardown[0]              \
    __attribute__((section(sec ".teardown")))

#define END_SUITE(sym)                                  \
    struct test_case _end_##sym##_cases[0]              \
    __attribute__((section(".data." #sym ".cases")))

#define construct_test_suite(name)              \
    _construct_test_suite(&(_suite_ ## name))

Suite *_construct_test_suite(struct test_suite*);
