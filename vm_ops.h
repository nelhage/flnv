#ifndef __FLNV_VM_OPS_H__
#define __FLNV_VM_OPS_H__

enum vm_opcode {
    OP_NOP = 0,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_CONS,
    OP_CAR,
    OP_SET_CAR,
    OP_CDR,
    OP_SET_CDR,
    OP_MAKE_VECTOR,
    OP_VECTOR_REF,
    OP_VECTOR_SET,
    OP_EXTEND_ENV,
    OP_ENV_PARENT,
    OP_ENV_LOOKUP,
    OP_ENV_REF,
    OP_ENV_SET,
    OP_CONS_P,
    OP_NUMBER_P,
    OP_VECTOR_P,
    OP_BOOLEAN_P,
    OP_NULL_P,
    OP_PROCEDURE_P,
    OP_MAKE_CLOSURE,
    OP_INVOKE_PROCEDURE,
    OP_EQ_P,
    OP_EQUAL,
    OP_LESS,
    OP_GREATER,
    OP_LEQ,
    OP_GEQ,
    OP_PUSH_INT,
    OP_BRANCH,
    OP_JMP,
    OP_JT,
    OP_PUSH_ADDR,
    OP_POP
};

/********************************************************************
 * Opcode formats:
 * OOOOOOO YYYYXXXX 0000ZZZZ
 * OOOOOOO YYYYXXXX IIIIIIII
 ********************************************************************/

#endif
