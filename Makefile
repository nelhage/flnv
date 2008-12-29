CC=gcc
CFLAGS=-g -Wall $(DEFS)
OBJECTS=gc.o symbol.o read.o

TEST_CFLAGS=$(shell pkg-config check --cflags)
TEST_LIBS=$(shell pkg-config check --libs)
TEST_LDFLAGS=
TEST_OBJECTS=tests.o
TESTER=tests

all: check

check: $(TESTER)
	./$<

$(TEST_OBJECTS): CFLAGS += $(TEST_CFLAGS)

$(TESTER): LDFLAGS += $(TEST_LDFLAGS)
$(TESTER): LDLIBS += $(TEST_LIBS)
$(TESTER): $(TEST_OBJECTS) $(OBJECTS)

clean:
	rm -f *.o $(TEST)

check-syntax:
	$(CC) $(CCFLAGS) -Wall -Wextra -fsyntax-only $(CHK_SOURCES)
