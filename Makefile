CC=gcc
CFLAGS=-g -Wall $(DEFS) -D_GNU_SOURCE
CPPFLAGS= -I.
OBJECTS=gc.o scgc.o symbol.o vm.o

TEST_CFLAGS=$(shell pkg-config check --cflags)
TEST_LIBS=$(shell pkg-config check --libs)
TEST_LDFLAGS=
TEST_OBJECTS=tests.o test/gc.o test/vm.o
TESTER=tests

SOURCES=$(OBJECTS:.o=.c) $(TEST_OBJECTS:.o=.c)

all: check

check: $(TESTER)
	./$<

$(TEST_OBJECTS): CFLAGS += $(TEST_CFLAGS)

$(TESTER): LDFLAGS += $(TEST_LDFLAGS)
$(TESTER): LDLIBS += $(TEST_LIBS)
$(TESTER): $(TEST_OBJECTS) $(OBJECTS)

clean:
	rm -f $(TESTER) $(OBJECTS) $(TEST_OBJECTS)

check-syntax:
	$(CC) $(CFLAGS) $(CPPFLAGS) -Wall -Wextra -fsyntax-only $(CHK_SOURCES)

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MT "$(<:.c=.o) $@" -MM $(CPPFLAGS) $< > $@

-include $(SOURCES:.c=.d)
