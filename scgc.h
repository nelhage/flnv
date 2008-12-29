#ifndef __FLNV_SC_GC_H__
#define __FLNV_SC_GC_H__

#include "gc.h"

/* Memory allocaton */
gc_handle sc_alloc_cons();
gc_handle sc_alloc_string(uint32_t len);
gc_handle sc_alloc_vector(uint32_t len);
gc_handle sc_alloc_symbol(uint32_t len);

gc_handle sc_make_string(char * s);
gc_handle sc_make_number(gc_int n);

/* Accesors */
gc_handle sc_car(gc_handle c);
gc_handle sc_cdr(gc_handle c);
void sc_set_car(gc_handle c, gc_handle val);
void sc_set_cdr(gc_handle c, gc_handle val);

char * sc_string_get(gc_handle s);
uint32_t sc_strlen(gc_handle s);

char* sc_symbol_name(gc_handle s);

gc_int sc_number(gc_handle n);

uint32_t sc_vector_len(gc_handle v);
gc_handle sc_vector_ref(gc_handle v, uint32_t n);
void sc_vector_set(gc_handle v, uint32_t n, gc_handle x);

/* Predicates */
int sc_consp(gc_handle c);
int sc_stringp(gc_handle c);
int sc_numberp(gc_handle c);
int sc_symbolp(gc_handle c);
int sc_vectorp(gc_handle c);

#endif
