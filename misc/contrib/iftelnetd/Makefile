# $Header: /users/lord/src/iftelnetd/Makefile,v 1.2 1995/09/09 11:28:08 lord Exp $

iftelnetd:	iftelnetd.c
	cc -O2 -o iftelnetd iftelnetd.c

clean:
	rm -f iftelnetd *.o

install: iftelnetd
	cp iftelnetd /usr/lib/ifmail
	chown fnet:uucp /usr/lib/ifmail/iftelnetd
	chmod a+rx /usr/lib/ifmail/iftelnetd
	
