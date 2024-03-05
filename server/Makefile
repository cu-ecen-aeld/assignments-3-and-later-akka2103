
CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?=-Wall -Werror -g

# Linker Flags
LDFLAGS ?= -pthread -lrt

all: aesdsocket

default: all

aesdsocket:
	$(CC) $(CFLAGS) $(LDFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm -f *.o aesdsocket
