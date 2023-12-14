# @file Makefile
# @author Kristijan Todorovic, 11806442
# @date 14.12.2023

CC     = gcc
DEFS   = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIC_C_SOURCE=200809L
CFLAGS = -std=c99 -pedantic -Wall $(DEFS) -g
TAR    = 3-http-todorovic.tar.gz

.PHONY: all clean

all:

clean:
    rm -rf *.o $(TAR)

pack:
    tar -cvzf $(TAR) *.c *.h Makefile
