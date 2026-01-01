CFLAGS = -Os -g -I.
LDFLAGS =

CSV = ../csvTrivialCallbackParser

# Include path for CSV parser
CFLAGS += -I$(CSV)

# debug output
#CFLAGS += -DDTC_DEBUG

# Example decision tables
EXAMPLES = power DisjunctiveNormalForm

# Phony targets
.PHONY: all examples examples-py clean clobber

# Targets
all: dtc

examples: $(EXAMPLES:=.h) $(EXAMPLES:=.c) test

examples-py: $(EXAMPLES:=.py)
	python3 test.py

clean:
	rm -f dtc.o
	rm -f power.o DisjunctiveNormalForm.o test.o

clobber: clean
	rm -f dtc
	rm -f $(EXAMPLES:=.h) $(EXAMPLES:=.c)
	rm -f $(EXAMPLES:=.py)
	rm -f $(EXAMPLES:=.psu)
	rm -f test

dtc: dtc.o $(CSV)/csv.o
	$(CC) $(LDFLAGS) -o $@ $^

# Generate pseudocode from decision table
%.psu: %.dtc dtc
	./dtc $< > $@

# Generate C header and implementation from pseudocode
# AWK generates both .h and .c in one invocation
%.h: %.psu C.awk
	awk -f C.awk $<

# .c file is generated as a side effect of .h generation
%.c: %.h
	@:

# Generate Python module from pseudocode
%.py: %.psu psu2py.py
	python3 psu2py.py $< > $@

# Example test program
test: test.o power.o DisjunctiveNormalForm.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
