CC=gcc

CFLAGS=-Wall -Werror
LIBS?=-lnfp
LDFLAGS>?=-L/opt/netronome/lib

OBJS = nfp-pciebench-helper.o

nfp-pciebench-helper: $(OBJS)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm -f *.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
