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
    OP_EQ_P,
    OP_EQUAL,
    OP_LESS,
    OP_GREATER,
    OP_LEQ,
    OP_GEQ,
    OP_LOAD_INT,
    OP_LOAD_NULL,
    OP_MOV,
    OP_BRANCH,
    OP_JMP,
    OP_JT,
    OP_CALL,
    OP_PUSH,
    OP_POP
};

/********************************************************************
 * Opcode formats:
 * OOOOOOO YYYYXXXX 0000ZZZZ
 * OOOOOOO YYYYXXXX IIIIIIII
 ********************************************************************/

#endif
