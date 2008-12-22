CFLAGS=-g -Wall $(DEFS) $(TEST_CFLAGS)
OBJECTS=gc.o symbol.o read.o

TEST_OBJECTS=$(OBJECTS) test.o
TEST_CFLAGS=-DBUILD_TEST
TESTER=tester

all: $(TESTER)

test: $(TESTER)
	./$<

$(TESTER): $(TEST_OBJECTS)
	gcc -o $@ $(TEST_OBJECTS) $(LDFLAGS)

clean:
	rm -f *.o $(TEST)

check-syntax:
	$(CC) $(CCFLAGS) -Wall -Wextra -fsyntax-only $(CHK_SOURCES)
