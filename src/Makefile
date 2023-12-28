# @file Makefile for http client and server
# @author Kristijan Todorovic, 11806442
# @date 14.12.2023

CC     = gcc
DEFS   = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIC_C_SOURCE=200809L
CFLAGS = -std=c99 -pedantic -Wall $(DEFS) -g
TAR    = 3-http-todorovic.tar.gz

.PHONY: all clean

all: client #server

client: client.o
	$(CC) -o client client.o

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

server: server.o
	$(CC) -o server server.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -rf *.o $(TAR) client server

pack:
	tar -cvzf $(TAR) client.c Makefile
