
IDIR=include
CC=gcc
CFLAGS=-I$(IDIR)

ODIR=.
LDIR=lib/libtree

LIBS=-lm

_DEPS = libtree.h utlist.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = avl.o bst.o rb.o splay.o
OBJ = $(patsubst %,$(LDIR)/%,$(_OBJ))


$(LDIR)/%.o: $(LDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

main: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS) main.c

port_iana:
	python parse_iana.py

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
