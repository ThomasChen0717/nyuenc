CC=gcc
CFLAGS= -g -O -pedantic -std=gnu17 -Wall -Werror -Wextra
LDFLAGS= -pthread  -lpthread 

.PHONY: all
all: nyuenc

nyuenc: nyuenc.o 

nyuenc.o: nyuenc.c 

.PHONY: clean
clean:
	rm -f *.o nyuenc
