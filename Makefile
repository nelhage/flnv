CFLAGS=-g -Wall $(DEFS)
OBJECTS=gc.o symbol.o read.o

TEST_CFLAGS=
TEST_LIBS=
TEST_LDFLAGS=
TEST_OBJECTS=tests.o /usr/lib/libcheck.a
TESTER=tests

all: check

check: $(TESTER)
	./$<

$(TEST_OBJECTS): CFLAGS += $(TEST_CFLAGS)

$(TESTER): LDFLAGS += $(TEST_LDFLAGS) $(foreach lib,$(TEST_LIBS), -l$(lib))
$(TESTER): $(TEST_OBJECTS) $(OBJECTS)

clean:
	rm -f *.o $(TEST)

check-syntax:
	$(CC) $(CCFLAGS) -Wall -Wextra -fsyntax-only $(CHK_SOURCES)
