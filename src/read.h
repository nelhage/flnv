#ifndef __MINISCHEME_READ__
#define __MINISCHEME_READ__

#include "gc.h"

struct in_stream {
    int (*getc)(struct in_stream * s);
    void (*ungetc)(struct in_stream * s, int c);

    void * user;
};

sc_val sc_read(struct in_stream * s);

#ifndef EOF
#define EOF -1
#endif

#endif /* !defined(__MINISCHEME_READ__) */
