IDIR =include
CC=gcc
CFLAGS=-I$(IDIR)
LDIR =lib

LIBS=-lm

_DEPS = utils.h netUtils.h httpUtils.h homeNet.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

DNS_DEPS = lib/dns/dns.h lib/dns/mappings.h lib/dns/output.h
SHA_DEPS = lib/sha/sha2.h

OBJ = src/utils.o src/netUtils.o src/store.o src/homeNet.o src/httpUtils.o main.o

SHA_OBJ = lib/sha/sha2.o

DNS_OBJ = lib/dns/codec.o lib/dns/mappings.o lib/dns/output.o


homenet: $(OBJ) libsha.a libdns.a
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/main.o: main.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

libsha.a: $(SHA_OBJ) $(SHA_DEPS)
	ar rcs libsha.a $(SHA_OBJ)

libdns.a: $(DNS_OBJ) $(DNS_DEPS)
	ar rcs libdns.a $(DNS_OBJ)

.PHONY: clean

clean:
	rm -f *.o $(OBJ) $(LIBS) homenet a.out