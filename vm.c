#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "vm_ops.h"
#include "gc.h"
#include "scgc.h"

#define die(fmt, ...) __die(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

void __die(char *file, int line, char *fmt, ...);

#define N_REGS     16
#define REG_BITS   4
#define STACK_SIZE 4096

gc_handle vm_regs[N_REGS];
gc_handle *vm_stack = NULL;
gc_handle *vm_stack_ptr = NULL;
uint8_t   *vm_ip;

void (*vm_error_handler)(char *file, int line, char *err);

void vm_relocate_hook() {
    int i;
    gc_handle *sp;

    for(i = 0; i < N_REGS; i++) {
        gc_relocate(&vm_regs[i]);
    }

    for(sp = vm_stack; sp < vm_stack_ptr; sp++) {
        gc_relocate(sp);
    }
}

void vm_init() {
    int i;

    if(vm_stack != NULL) {
        free(vm_stack);
    }

    vm_error_handler = NULL;

    vm_stack = malloc(STACK_SIZE * sizeof(gc_handle));
    vm_stack_ptr = vm_stack;

    for(i = 0; i < N_REGS; i++) {
        vm_regs[i] = NIL;
    }

    gc_register_gc_root_hook(vm_relocate_hook);
}

static inline void type_check_reg(uint8_t reg, int (*pred)(gc_handle), char *fn) {
    if(!pred(vm_regs[reg])) {
        die("TYPECHECK ERROR -- %s", fn);
    }
}

void vm_set_ip(uint8_t *codeptr) {
    vm_ip = codeptr;
}

void vm_step_one() {
    uint8_t r1, r2, r3;
    uint32_t iarg;

#define REG_ARGS_2 do {                         \
        r1 = *vm_ip & 0x0F;                     \
        r2 = *vm_ip >> 4;                       \
        vm_ip++;                                \
    } while(0)

#define REG_ARGS_3 do {                         \
        REG_ARGS_2;                             \
        r3 = *vm_ip & 0x0F;                     \
        vm_ip++;                                \
    } while(0)

#define INT_ARG do {                            \
        iarg = *((uint32_t*)vm_ip);             \
        vm_ip += sizeof(uint32_t);              \
    } while(0)

#define TC1(pred, name) do {                    \
        type_check_reg(r1, sc_##pred##p, name); \
    } while (0)

#define TC2(pred, name) do {                    \
        type_check_reg(r1, sc_##pred##p, name); \
        type_check_reg(r2, sc_##pred##p, name); \
    } while (0)

    switch(*(vm_ip++)) {
    case OP_NOP:
        break;

    case OP_ADD:
        REG_ARGS_3;
        TC2(number, "ADD");

        vm_regs[r3] = sc_make_number(sc_number(vm_regs[r1]) +
                                     sc_number(vm_regs[r2]));
        break;

    case OP_SUB:
        REG_ARGS_3;
        TC2(number, "SUB");

        type_check_reg(r1, sc_numberp, "SUB");
        type_check_reg(r2, sc_numberp, "SUB");
        vm_regs[r3] = sc_make_number(sc_number(vm_regs[r1]) -
                                     sc_number(vm_regs[r2]));
        break;
    case OP_MUL:
        REG_ARGS_3;
        TC2(number, "SUB");

        type_check_reg(r1, sc_numberp, "MUL");
        type_check_reg(r2, sc_numberp, "MUL");
        vm_regs[r3] = sc_make_number(sc_number(vm_regs[r1]) *
                                     sc_number(vm_regs[r2]));
        break;
    case OP_DIV:
        REG_ARGS_3;
        TC2(number, "SUB");

        type_check_reg(r1, sc_numberp, "DIV");
        type_check_reg(r2, sc_numberp, "DIV");
        vm_regs[r3] = sc_make_number(sc_number(vm_regs[r1]) /
                                     sc_number(vm_regs[r2]));
        break;
    case OP_CONS:
        REG_ARGS_3;

        vm_regs[r3] = sc_alloc_cons();
        sc_set_car(vm_regs[r3], vm_regs[r1]);
        sc_set_cdr(vm_regs[r3], vm_regs[r2]);
        break;

    case OP_CAR:
        REG_ARGS_2;
        TC1(cons, "CAR");

        vm_regs[r2] = sc_car(vm_regs[r1]);
        break;

    case OP_CDR:
        REG_ARGS_2;
        TC1(cons, "CDR");

        vm_regs[r2] = sc_cdr(vm_regs[r1]);
        break;

    case OP_SET_CAR:
        REG_ARGS_2;
        TC1(cons, "SET-CAR");

        sc_set_car(vm_regs[r2], vm_regs[r1]);
        break;

    case OP_SET_CDR:
        REG_ARGS_2;
        TC1(cons, "SET-CDR");

        sc_set_cdr(vm_regs[r2], vm_regs[r1]);
        break;

    case OP_LOAD_INT:
        REG_ARGS_2;
        INT_ARG;

        vm_regs[r1] = sc_make_number(iarg);
        break;
    }
}

gc_handle vm_read_reg(uint8_t regnum) {
    if(regnum >= N_REGS) {
        die("Bad register: %d", regnum);
    }

    return vm_regs[regnum];
}

void __die(char *file, int line, char *fmt, ...)
{
    va_list ap;

    if(vm_error_handler) {
        char *buf;

        va_start(ap, fmt);
        vasprintf(&buf, fmt, ap);
        va_end(ap);

        vm_error_handler(file, line, buf);
    } else {
        fprintf(stderr, "ERROR[%s:%d]: ", file, line);

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);

        fprintf(stderr, "\n");
    }
    exit(-1);
}
