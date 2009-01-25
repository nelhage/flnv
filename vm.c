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

typedef struct vm_env {
    gc_chunk  header;
    gc_handle parent;
    gc_handle names;
    uint32_t  n_vars;
    gc_handle vars[];
} vm_env;

uint32_t vm_len_env(vm_env *env) {
    return sizeof(vm_env)/sizeof(gc_handle) + env->n_vars;
}

void vm_relocate_env(vm_env *env) {
    unsigned int i;

    gc_relocate(&env->parent);

    for(i=0;i<env->n_vars;i++) {
        gc_relocate(&env->vars[i]);
    }
}

static struct gc_ops vm_env_ops = {
    .op_relocate  = (gc_relocate_op)vm_relocate_env,
    .op_len       = (gc_len_op)vm_len_env
};

gc_handle vm_alloc_env(uint32_t n_vars, gc_handle *parent) {
    vm_env *env = gc_alloc(&vm_env_ops, sizeof(vm_env)/sizeof(gc_handle) + n_vars);
    env->names  = NIL;
    env->parent = *parent;
    env->n_vars = n_vars;
    return gc_tag_pointer(env);
}

int vm_envp(gc_handle h) {
    if(NILP(h)) return 1;
    if(!gc_pointerp(h)) return 0;
    return UNTAG_PTR(h, gc_chunk)->ops == &vm_env_ops;
}

gc_handle vm_env_parent(gc_handle h) {
    return UNTAG_PTR(h, vm_env)->parent;
}

gc_handle vm_env_lookup_name(gc_handle h, gc_handle name) {
    vm_env *e = UNTAG_PTR(h, vm_env);
    uint32_t i;

    if(NILP(e->names)) {
        return NIL;
    }
    for(i = 0; i < sc_vector_len(e->names); i++) {
        if(sc_vector_ref(e->names, i) == name) {
            return e->vars[i];
        }
    }
    return NIL;
}

void vm_env_set_named(gc_handle env, uint32_t idx, gc_handle name, gc_handle val) {
    vm_env *e = UNTAG_PTR(env, vm_env);
    gc_handle vec = NIL;

    if(idx >= e->n_vars) {
        die("ENV-SET-NAMED: Index %d out of bounds (size %d)", idx, e->n_vars);
    }
    e->vars[idx] = val;

    gc_register_roots(&env, &name, &vec, NULL);
    vec = sc_alloc_vector(e->n_vars);
    sc_vector_set(vec, idx, name);
    UNTAG_PTR(env, vm_env)->names = vec;
    gc_pop_roots();
}

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

void vm_push(gc_handle h) {
    *(vm_stack_ptr++) = h;
}

gc_handle vm_pop() {
    vm_stack_ptr--;
    return *(vm_stack_ptr);
}

void vm_step_one() {
    uint8_t r1, r2, r3;
    uint32_t iarg;

#define REG_ARGS_1 do {                         \
        r1 = *vm_ip & 0x0F;                     \
        vm_ip++;                                \
    } while(0)                                  \

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
        type_check_reg(r1, pred, name); \
    } while (0)

#define TC2(pred, name) do {                    \
        type_check_reg(r1, pred, name); \
        type_check_reg(r2, pred, name); \
    } while (0)

    switch(*(vm_ip++)) {
    case OP_NOP:
        break;

    case OP_ADD:
        REG_ARGS_3;
        TC2(sc_numberp, "ADD");

        vm_regs[r3] = sc_make_number(sc_number(vm_regs[r1]) +
                                     sc_number(vm_regs[r2]));
        break;

    case OP_SUB:
        REG_ARGS_3;
        TC2(sc_numberp, "SUB");

        vm_regs[r3] = sc_make_number(sc_number(vm_regs[r1]) -
                                     sc_number(vm_regs[r2]));
        break;
    case OP_MUL:
        REG_ARGS_3;
        TC2(sc_numberp, "SUB");

        vm_regs[r3] = sc_make_number(sc_number(vm_regs[r1]) *
                                     sc_number(vm_regs[r2]));
        break;
    case OP_DIV:
        REG_ARGS_3;
        TC2(sc_numberp, "SUB");

        vm_regs[r3] = sc_make_number(sc_number(vm_regs[r1]) /
                                     sc_number(vm_regs[r2]));
        break;

        /* Pairs */
    case OP_CONS:
        REG_ARGS_3;

        vm_regs[r3] = sc_alloc_cons();
        sc_set_car(vm_regs[r3], vm_regs[r1]);
        sc_set_cdr(vm_regs[r3], vm_regs[r2]);
        break;

    case OP_CAR:
        REG_ARGS_2;
        TC1(sc_consp, "CAR");

        vm_regs[r2] = sc_car(vm_regs[r1]);
        break;

    case OP_CDR:
        REG_ARGS_2;
        TC1(sc_consp, "CDR");

        vm_regs[r2] = sc_cdr(vm_regs[r1]);
        break;

    case OP_SET_CAR:
        REG_ARGS_2;
        TC1(sc_consp, "SET-CAR");

        sc_set_car(vm_regs[r2], vm_regs[r1]);
        break;

    case OP_SET_CDR:
        REG_ARGS_2;
        TC1(sc_consp, "SET-CDR");

        sc_set_cdr(vm_regs[r2], vm_regs[r1]);
        break;

        /* Vectors */
    case OP_MAKE_VECTOR:
        REG_ARGS_2;
        TC1(sc_numberp, "MAKE-VECTOR");

        vm_regs[r2] = sc_alloc_vector(sc_number(vm_regs[r1]));
        break;

    case OP_VECTOR_REF: {
        unsigned int idx;
        gc_handle vec;

        REG_ARGS_3;
        TC1(sc_vectorp, "VECTOR-REF");
        type_check_reg(r2, sc_numberp, "VECTOR-REF");

        vec = vm_regs[r1];
        idx = sc_number(vm_regs[r2]);

        if(idx >= sc_vector_len(vec)) {
            die("Index %d out-of-bounds (len %d) in VECTOR-REF", idx, sc_vector_len(vec));
        }

        vm_regs[r3] = sc_vector_ref(vec, idx);
        break;
    }

    case OP_VECTOR_SET: {
        unsigned int idx;
        gc_handle vec;

        REG_ARGS_3;
        type_check_reg(r2, sc_numberp, "VECTOR-SET");
        type_check_reg(r3, sc_vectorp, "VECTOR-SET");

        vec = vm_regs[r3];
        idx = sc_number(vm_regs[r2]);

        if(idx >= sc_vector_len(vec)) {
            die("Index %d out-of-bounds (len %d) in VECTOR-SET", idx, sc_vector_len(vec));
        }

        sc_vector_set(vec, idx, vm_regs[r3]);
        break;
    }
        /* Environments */

    case OP_EXTEND_ENV:
        REG_ARGS_2;
        INT_ARG;

        TC1(vm_envp, "EXTEND-ENV");

        vm_regs[r2] = vm_alloc_env(iarg, &vm_regs[r1]);
        break;

    case OP_ENV_PARENT:
        REG_ARGS_2;

        TC1(vm_envp, "ENV-PARENT");

        if(NILP(vm_regs[r1])) {
            die("ENV-PARENT on a NIL environment");
        } else {
            vm_regs[r2] = vm_env_parent(vm_regs[r1]);
        }
        break;

    case OP_ENV_LOOKUP:
        REG_ARGS_3;
        if(!vm_envp(vm_regs[r2])) {
            die("TYPECHECK ERROR -- ENV-LOOKUP");
        }
        vm_regs[r3] = vm_env_lookup_name(vm_regs[r2], vm_regs[r1]);
        break;

    case OP_ENV_REF:
        REG_ARGS_2;
        INT_ARG;

        TC1(vm_envp, "ENV-REF");
        vm_regs[r2] = UNTAG_PTR(vm_regs[r1], vm_env)->vars[iarg];
        break;

    case OP_ENV_SET:
        REG_ARGS_2;
        INT_ARG;

        if(!vm_envp(vm_regs[r2])) {
            die("TYPECHECK ERROR -- ENV-SET");
        }

        UNTAG_PTR(vm_regs[r1], vm_env)->vars[iarg] = vm_regs[r2];
        break;

        /* Predicates */

    case OP_CONS_P:
        REG_ARGS_2;

        vm_regs[r2] = sc_consp(vm_regs[r1]) ? sc_true : sc_false;
        break;

    case OP_NUMBER_P:
        REG_ARGS_2;

        vm_regs[r2] = sc_numberp(vm_regs[r1]) ? sc_true : sc_false;
        break;

    case OP_VECTOR_P:
        REG_ARGS_2;

        vm_regs[r2] = sc_vectorp(vm_regs[r1]) ? sc_true : sc_false;
        break;

    case OP_BOOLEAN_P:
        REG_ARGS_2;

        vm_regs[r2] = sc_booleanp(vm_regs[r1]) ? sc_true : sc_false;
        break;

    case OP_NULL_P:
        REG_ARGS_2;

        vm_regs[r2] = NILP(vm_regs[r1]) ? sc_true : sc_false;
        break;

        /* VM control ops*/
    case OP_LOAD_INT:
        REG_ARGS_1;
        INT_ARG;

        vm_regs[r1] = sc_make_number(iarg);
        break;

    case OP_LOAD_NULL:
        REG_ARGS_1;

        vm_regs[r1] = NIL;
        break;

    case OP_MOV:
        REG_ARGS_1;

        vm_regs[r2] = vm_regs[r1];
        break;

    case OP_BRANCH:
        INT_ARG;

        vm_ip += iarg;
        break;

    case OP_JMP:
        REG_ARGS_1;
        type_check_reg(r1, gc_externalp, "JMP");

        vm_ip = gc_untag_external(r1);
        break;

    case OP_JT:
        REG_ARGS_1;
        INT_ARG;

        if(vm_regs[r1] != sc_false) {
            vm_ip += iarg;
        }
        break;

    case OP_CALL:
        INT_ARG;

        vm_push(gc_tag_external(vm_ip));
        vm_ip += iarg;
        break;

    case OP_PUSH:
        REG_ARGS_1;

        vm_push(vm_regs[r1]);
        break;

    case OP_POP:
        REG_ARGS_1;

        vm_regs[r1] = vm_pop();
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
