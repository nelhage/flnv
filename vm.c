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

gc_handle vm_env_reg;
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

typedef struct vm_closure {
    gc_chunk header;
    gc_handle env;
    uint8_t   *code;
    int32_t   n_args;
} vm_closure;

typedef struct vm_builtin {
    gc_chunk header;
    void (*code)(void);
} vm_builtin;

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

uint32_t vm_len_closure(vm_closure *c UNUSED) {
    return sizeof(vm_closure)/sizeof(gc_handle);
}

void vm_relocate_closure(vm_closure *c UNUSED) {
    gc_relocate(&c->env);
}

uint32_t vm_len_builtin(vm_builtin *b UNUSED) {
    return sizeof(vm_builtin)/sizeof(gc_handle);
}


static struct gc_ops vm_env_ops = {
    .op_relocate  = (gc_relocate_op)vm_relocate_env,
    .op_len       = (gc_len_op)vm_len_env
};

static struct gc_ops vm_closure_ops = {
    .op_relocate = (gc_relocate_op)vm_relocate_closure,
    .op_len      = (gc_len_op)vm_len_closure
};

static struct gc_ops vm_builtin_ops = {
    .op_relocate = (gc_relocate_op)gc_relocate_nop,
    .op_len      = (gc_len_op)vm_len_builtin
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

int vm_procedurep(gc_handle h) {
    return gc_pointerp(h) && !NILP(h)
        && (UNTAG_PTR(h, gc_chunk)->ops == &vm_closure_ops ||
            UNTAG_PTR(h, gc_chunk)->ops == &vm_builtin_ops);
}

void vm_relocate_hook() {
    gc_handle *sp;

    gc_relocate(&vm_env_reg);

    for(sp = vm_stack; sp < vm_stack_ptr; sp++) {
        gc_relocate(sp);
    }
}

void vm_init() {
    if(vm_stack != NULL) {
        free(vm_stack);
    }

    vm_error_handler = NULL;

    vm_stack = malloc(STACK_SIZE * sizeof(gc_handle));
    vm_stack_ptr = vm_stack;

    vm_env_reg = NIL;

    gc_register_gc_root_hook(vm_relocate_hook);
}

void vm_set_ip(uint8_t *codeptr) {
    vm_ip = codeptr;
}

inline void vm_push(gc_handle h) {
    *(vm_stack_ptr++) = h;
}

inline gc_handle vm_pop() {
    vm_stack_ptr--;
    return *(vm_stack_ptr);
}

inline gc_handle vm_top() {
    return *(vm_stack_ptr-1);
}

inline gc_handle vm_pop_tc(int (*predicate)(gc_handle), char *why) {
    gc_handle top = vm_pop();
    if(!predicate(top)) {
        die("TYPECHECK ERROR -- %s", why);
    }
    return top;
}

void vm_step_one() {
    uint32_t iarg;

#define INT_ARG do {                            \
        iarg = *((uint32_t*)vm_ip);             \
        vm_ip += sizeof(uint32_t);              \
    } while(0)

    switch((enum vm_opcode)*(vm_ip++)) {
    case OP_NOP:
        break;

    case OP_ADD: {
        gc_handle rhs = vm_pop_tc(sc_numberp, "ADD");
        gc_handle lhs = vm_pop_tc(sc_numberp, "ADD");
        vm_push(sc_add(lhs, rhs));
        break;
    }

    case OP_SUB: {
        gc_handle rhs = vm_pop_tc(sc_numberp, "SUB");
        gc_handle lhs = vm_pop_tc(sc_numberp, "SUB");
        vm_push(sc_sub(lhs, rhs));
        break;
    }

    case OP_MUL: {
        gc_handle rhs = vm_pop_tc(sc_numberp, "MUL");
        gc_handle lhs = vm_pop_tc(sc_numberp, "MUL");
        vm_push(sc_mul(lhs, rhs));
        break;
    }

    case OP_DIV: {
        gc_handle rhs = vm_pop_tc(sc_numberp, "DIV");
        gc_handle lhs = vm_pop_tc(sc_numberp, "DIV");
        vm_push(sc_div(lhs, rhs));
        break;
    }

        /* Pairs */
    case OP_CONS: {
        gc_handle cons = sc_alloc_cons();
        sc_set_cdr(cons, vm_pop());
        sc_set_car(cons, vm_pop());
        vm_push(cons);
        break;
    }

    case OP_CAR:
        vm_push(sc_car(vm_pop_tc(sc_consp, "CAR")));
        break;

    case OP_CDR:
        vm_push(sc_cdr(vm_pop_tc(sc_consp, "CDR")));
        break;

    case OP_SET_CAR: {
        gc_handle car = vm_pop();
        sc_set_car(vm_pop_tc(sc_consp, "SET-CAR"), car);
        break;
    }

    case OP_SET_CDR: {
        gc_handle cdr = vm_pop();
        sc_set_cdr(vm_pop_tc(sc_consp, "SET-CDR"), cdr);
        break;
    }

        /* Vectors */
    case OP_MAKE_VECTOR: {
        int len = sc_number(vm_pop_tc(sc_numberp, "MAKE-VECTOR"));
        vm_push(sc_alloc_vector(len));
        break;
    }

    case OP_VECTOR_REF: {
        unsigned int idx;
        gc_handle vec;

        idx = sc_number(vm_pop_tc(sc_numberp, "VECTOR-REF"));
        vec = vm_pop_tc(sc_vectorp, "VECTOR-REF");

        if(idx >= sc_vector_len(vec)) {
            die("Index %d out-of-bounds (len %d) in VECTOR-REF", idx, sc_vector_len(vec));
        }

        vm_push(sc_vector_ref(vec, idx));
        break;
    }

    case OP_VECTOR_SET: {
        unsigned int idx;
        gc_handle vec, val;

        val = vm_pop();
        idx = sc_number(vm_pop_tc(sc_numberp, "VECTOR-SET"));
        vec = vm_pop_tc(sc_vectorp, "VECTOR-SET");

        if(idx >= sc_vector_len(vec)) {
            die("Index %d out-of-bounds (len %d) in VECTOR-SET", idx, sc_vector_len(vec));
        }

        sc_vector_set(vec, idx, val);
        break;
    }
        /* Environments */

    case OP_EXTEND_ENV: {
        int size = sc_number(vm_pop_tc(sc_numberp, "EXTEND-ENV"));
        int i;

        gc_handle env = vm_alloc_env(size, &vm_env_reg);
        for(i = size-1; i >=0; i--) {
            UNTAG_PTR(env, vm_env)->vars[i] = vm_pop();
        }
        break;
    }

    case OP_ENV_PARENT: {
        vm_env_reg = vm_env_parent(vm_env_reg);
        break;
    }

    case OP_ENV_LOOKUP: {
        gc_handle name = vm_pop();
        gc_handle val = NIL;
        gc_handle env = vm_env_reg;
        while(NILP(val) && !NILP(env)) {
            val = vm_env_lookup_name(env, name);
            env = vm_env_parent(env);
        }
        vm_push(val);
        break;
    }

    case OP_ENV_REF: {
        int frame  = sc_number(vm_pop_tc(sc_numberp, "ENV-REF"));
        gc_int idx = sc_number(vm_pop_tc(sc_numberp, "ENV-REF"));
        gc_handle env = vm_env_reg;
        vm_env    *envp;
        while(frame-- && !NILP(env)) {
            env = vm_env_parent(env);
        }
        if(NILP(env)) {
            die("Frame out of bounds -- ENV-REF");
        }

        envp = UNTAG_PTR(env, vm_env);
        if((uint32_t)idx >= envp->n_vars) {
            die("Environment reference out of bounds -- ENV-REF");
        }

        vm_push(envp->vars[idx]);
        break;
    }

    case OP_ENV_SET: {
        int frame  = sc_number(vm_pop_tc(sc_numberp, "ENV-SET"));
        gc_int idx = sc_number(vm_pop_tc(sc_numberp, "ENV-SET"));
        gc_handle val = vm_pop();
        gc_handle env = vm_env_reg;
        vm_env    *envp;
        while(frame-- && !NILP(env)) {
            env = vm_env_parent(env);
        }
        if(NILP(env)) {
            die("Frame out of bounds -- ENV-REF");
        }

        envp = UNTAG_PTR(env, vm_env);
        if((uint32_t)idx >= envp->n_vars) {
            die("Environment reference out of bounds -- ENV-REF");
        }

        envp->vars[idx] = val;
        break;
    }
        /* Predicates */

#define VM_PREDICATE(op, pred)                                  \
        case OP_##op##_P:                                       \
            vm_push(pred(vm_pop()) ? sc_true : sc_false);       \
        break;

        VM_PREDICATE(CONS, sc_consp);
        VM_PREDICATE(NUMBER, sc_numberp);
        VM_PREDICATE(VECTOR, sc_vectorp);
        VM_PREDICATE(BOOLEAN, sc_booleanp);
        VM_PREDICATE(NULL, NILP);
        VM_PREDICATE(PROCEDURE, vm_procedurep);

    case OP_INVOKE_PROCEDURE:
        die("Unimplemented!");

        break;

        /* VM control ops*/
    case OP_PUSH_INT:
        INT_ARG;

        vm_push(sc_make_number(iarg));
        break;

    case OP_BRANCH:
        INT_ARG;

        vm_ip += iarg;
        break;

    case OP_JMP:
        vm_ip = gc_untag_external(vm_pop_tc(gc_externalp, "JMP"));
        break;

    case OP_JT:
        INT_ARG;

        if(vm_pop() != sc_false) {
            vm_ip += iarg;
        }
        break;

    case OP_PUSH_ADDR:
        INT_ARG;

        vm_push(gc_tag_external(vm_ip + iarg));
        break;

    case OP_POP:
        vm_pop();
        break;

    default:
        die("Unknown opcode %02X", *(vm_ip - 1));
    }
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
