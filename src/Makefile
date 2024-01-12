# @file Makefile for http client and server
# @author Kristijan Todorovic, 11806442
# @date 14.12.2023

CC     = gcc
DEFS   = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c99 -pedantic -Wall -g #$(DEFS)
TARC   = 3-http_client-todorovic.tar.gz
TARS   = 3-http_server-todorovic.tar.gz

.PHONY: all clean

all: server client

client: client.o
	$(CC) -o client client.o

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

server: server.o
	$(CC) -o server server.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -rf *.o $(TARC) $(TARS) client server

packc:
	tar -cvzf $(TARC) client.c Makefile

packs:
	tar -cvzf $(TARS) server.c Makefile
