CFLAGS=-g -Wall $(DEFS) $(TEST_CFLAGS)
OBJECTS=gc.o symbol.o stack.o read.o

TEST_OBJECTS=$(OBJECTS) test.o
TEST_CFLAGS=-DBUILD_TEST
TEST=test

all: $(TEST)

$(TEST): $(TEST_OBJECTS)
	gcc -o $@ $(TEST_OBJECTS) $(LDFLAGS)

clean:
	rm -f *.o $(TEST)
