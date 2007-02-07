#ifndef __MINISCHEME_GC__
#define __MINISCHEME_GC__

#include <stdint.h>

typedef uint32_t* sc_val;

/* Accesors */

sc_val sc_car(sc_val c);
sc_val sc_cdr(sc_val c);
void sc_set_car(sc_val c, sc_val val);
void sc_set_cdr(sc_val c, sc_val val);

char * sc_string(sc_val s);
uint32_t sc_strlen(sc_val s);

char* sc_symbol_name(sc_val s);

int sc_number(sc_val n);

uint32_t sc_vector_len(sc_val v);
sc_val sc_vector_ref(sc_val v, uint32_t n);
void sc_vector_set(sc_val v, uint32_t n, sc_val x);

/* Predicates */
int sc_consp(sc_val c);
int sc_stringp(sc_val c);
int sc_numberp(sc_val c);
int sc_symbolp(sc_val c);
int sc_vectorp(sc_val c);

/* Memory allocaton */
sc_val gc_alloc_cons();
sc_val gc_alloc_string(uint32_t len);
sc_val gc_alloc_vector(uint32_t len);
sc_val gc_alloc_symbol(uint32_t len);

sc_val gc_make_string(char * s);
sc_val sc_make_number(int n);

/* GC control */
void gc_init();
void gc_gc();
void gc_realloc(uint32_t need_mem);
uint32_t gc_free_mem();


#define NIL          ((sc_val)0)
#define NILP(x)      (!(x))

extern sc_val _sc_regs[];
#define SC_REG(n) _sc_regs[(n)]
#define SC_NREGS 16

/* The scratch register -- not guaranteed to be preserved across any
   public function calls. */
#define SCRATCH         0

#endif /* !defined(__MINISCHEME_GC__)*/
