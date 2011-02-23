# Makefile for the "g3 tools" subtree
# of mgetty+sendfax
#
# SCCS: @(#)Makefile	1.2 94/01/13 Copyright (c) Gert Doering
#

CC=cc
CFLAGS=-O -I../../../iflib

OBJS= fidonet.o route.o
UTLIB= ../../../iflib/utlib.a

all: fidonet

fidonet: $(OBJS)
	 $(CC) -o fidonet $(OBJS) $(UTLIB) $(CFLAGS)

route.o:	route.c
fidonet.o:	fidonet.c

clean:
	rm -f *.o

install: all

