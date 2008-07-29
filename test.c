#include "gc.h"
#include "read.h"
#include "stack.h"
#include "symbol.h"

extern void test_gc();
extern void test_read();
extern void test_stack();
extern void test_symbol();

int main() {
    gc_init();
    obarray_init();

    test_gc();
    test_read();
    test_stack();
    test_symbol();

    return 0;
}
