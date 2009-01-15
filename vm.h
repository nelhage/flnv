#ifndef __FLNV_VM_H__
#define __FLNV_VM_H__

#include "gc.h"

void vm_init();
void vm_step_one();
void vm_set_ip(uint8_t *codeptr);

#endif

