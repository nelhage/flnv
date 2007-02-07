#include "read.h"
#include "stack.h"
#include <string.h>
#include <assert.h>

char * symbol_special = "+-/*:.!?<>";

int isws(int c) {
    return c <= ' ';
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

sc_val sc_read_number(struct in_stream * s);
sc_val sc_read_symbol(struct in_stream * s);
sc_val sc_read_string(struct in_stream * s);
sc_val sc_read_pair(struct in_stream * s, int recursive);

sc_val sc_read_internal(struct in_stream * s, int recursive);

int read_getc(struct in_stream * s, int skipws) {
    int c = s->getc(s);
    if(c == EOF) return EOF;
    if(c == '#') {
        while(1) {
            c = s->getc(s);
            if(c == EOF) return EOF;
            if(c == '\n') break;
        }
    }
    if(isws(c)) {
        while(1) {
            c = s->getc(s);
            if(c == EOF) return EOF;
            if(!isws(c)) {
                if(skipws) return c;
                s->ungetc(s,c);
                return ' ';
            }
        }
    }
    return c;
}

sc_val sc_read(struct in_stream * s) {
    return sc_read_internal(s, 0);
}

sc_val sc_read_internal(struct in_stream * s, int recursive) {
    int c;
    c = read_getc(s, 1);
    if(c == EOF) return NIL;
    if(c >= '0' && c <='9') {
        s->ungetc(s, c);
        return sc_read_number(s);
    }
    if(isalpha(c) || strchr(symbol_special, c)) {
        s->ungetc(s, c);
        return sc_read_symbol(s);
    }
    if(c == '"') {
        return sc_read_string(s);
    }
    if(c == '(') {
        return sc_read_pair(s, recursive);
    }
    return NIL;
}

sc_val sc_read_number(struct in_stream * s) {
    uint32_t n = 0;
    int c;
    while((c = read_getc(s, 0)) != EOF) {
        if(!isdigit(c)) {
            s->ungetc(s,c);
            break;
        }
        n *= 10;
        n += c - '0';
    }
    return sc_make_number(n);
}

sc_val sc_read_symbol(struct in_stream * s) {
    uint32_t mlen = 10;
    sc_val str = gc_alloc_string(mlen);
    char * buf = sc_string(str);
    int i = 0;
    int c;
    while((c = read_getc(s, 0)) != EOF) {
        if(!(isalpha(c) || isdigit(c) ||
             strchr(symbol_special, c))) {
            s->ungetc(s,c);
            break;
        }
        if(i == mlen) {
            SC_REG(SCRATCH) = str;
            mlen <<= 1;
            str = gc_alloc_string(mlen);
            strcpy(sc_string(str), sc_string(SC_REG(SCRATCH)));
            buf = sc_string(str);
        }
        buf[i++] = c;
    }
    sc_val sym = gc_alloc_symbol(sc_strlen(str));
    strcpy(sc_symbol_name(sym), sc_string(str));
    return sym;
}

sc_val sc_read_string(struct in_stream * s) {
    uint32_t mlen = 10;
    sc_val str = gc_alloc_string(mlen);
    char * buf = sc_string(str);
    int i = 0;
    int c;
    while((c = s->getc(s)) != EOF) {
        if(c == '"') break;
        if(c == '\\') {
            c = s->getc(s);
            assert(c != EOF);
            switch(c) {
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 't': c = '\t'; break;
            case '\\': c = '\\'; break;
            case '"': c = '"'; break;
            }
        }
        if(i == mlen) {
            SC_REG(SCRATCH) = str;
            mlen <<= 1;
            str = gc_alloc_string(mlen);
            strcpy(sc_string(str), sc_string(SC_REG(SCRATCH)));
            buf = sc_string(str);
        }
        buf[i++] = c;
    }
    return str;
}

sc_val sc_read_pair(struct in_stream * s, int recursive) {
    /* Read the car */
    SC_REG(0) = sc_read_internal(s, 0);
    /* Save it on the stack */
    stack_push(STACK, 0);
    int c = read_getc(s, 1);
    sc_val cdr;
    if(c == '.') {
        /* Read a dotted pair */
        cdr = sc_read_internal(s, 0);
    } else if(c == ')') {
        s->ungetc(s, c);
        cdr = NIL;
    } else if(c == EOF) {
        assert(!"sc_read_pair: Invalid read syntax");
    } else {
        s->ungetc(s, c);
        cdr = sc_read_pair(s, 1);
    }
    SC_REG(0) = cdr;
    sc_val pair = gc_alloc_cons();
    sc_set_car(pair, stack_pop(STACK));
    sc_set_cdr(pair, SC_REG(0));
    c = s->getc(s);
    if(c == ')') {
        if(recursive) s->ungetc(s, c);
        return pair;
    }
    /* XXX TODO ERROR HANDLING */
    return pair;
}

#ifdef BUILD_TEST

#include <stdio.h>
#include <malloc.h>

int str_getc(struct in_stream * s) {
    char c = *((char*)s->user++);
    if(c == 0) return EOF;
    return c;
}

void str_ungetc(struct in_stream * s, int c) {
    s->user--;
    *((char*)s->user) = c;
}

sc_val read_from_string(char * s) {
    char * s2 = strdup(s);
    struct in_stream str;
    str.getc = str_getc;
    str.ungetc = str_ungetc;
    str.user = s2;
    sc_val v = sc_read(&str);
    free(s2);
    return v;
}

void test_read(void) {
    sc_val v = read_from_string("1234");

    assert(sc_numberp(v));
    assert(sc_number(v) == 1234);

    v = read_from_string("hello");

    assert(sc_symbolp(v));
    assert(!strcmp(sc_symbol_name(v), "hello"));

    v = read_from_string("a-painfully-long-symbol:foo*bar*baz");
    assert(sc_symbolp(v));
    assert(!strcmp(sc_symbol_name(v), "a-painfully-long-symbol:foo*bar*baz"));

    v = read_from_string("/");
    assert(sc_symbolp(v));
    assert(!strcmp(sc_symbol_name(v), "/"));

    v = read_from_string("\"Hello, World\"");
    assert(sc_stringp(v));
    assert(!strcmp(sc_string(v), "Hello, World"));

    v = read_from_string("# This is a comment \n foo # More comments");
    assert(sc_symbolp(v));
    assert(!strcmp(sc_symbol_name(v), "foo"));

    v = read_from_string("\"\\n\\r\\t\\\\\\\"\'\"");
    assert(sc_stringp(v));
    assert(!strcmp(sc_string(v), "\n\r\t\\\"\'"));

    v = read_from_string("(a . b)");
    assert(sc_consp(v));
    assert(sc_symbolp(sc_car(v)));
    assert(!strcmp(sc_symbol_name(sc_car(v)), "a"));
    assert(sc_symbolp(sc_cdr(v)));
    assert(!strcmp(sc_symbol_name(sc_cdr(v)), "b"));

    v = read_from_string("(a b c)");
    assert(sc_consp(v));
    assert(sc_symbolp(sc_car(v)));
    assert(!strcmp(sc_symbol_name(sc_car(v)), "a"));
    assert(sc_consp(sc_cdr(v)));
    assert(sc_symbolp(sc_car(sc_cdr(v))));
    assert(!strcmp(sc_symbol_name(sc_car(sc_cdr(v))), "b"));
    assert(sc_symbolp(sc_car(sc_cdr(sc_cdr(v)))));
    assert(!strcmp(sc_symbol_name(sc_car(sc_cdr(sc_cdr(v)))), "c"));
    assert(NILP(sc_cdr(sc_cdr(sc_cdr(v)))));

    v = read_from_string("((a b) c)");
    assert(sc_consp(v));
    assert(sc_consp(sc_car(v)));
    assert(sc_consp(sc_cdr(v)));
    assert(sc_symbolp(sc_car(sc_cdr(sc_car(v)))));
    assert(sc_symbolp(sc_car(sc_car(v))));
}

#endif
