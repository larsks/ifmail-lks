.include "MAKE.CONFIG"

VERSION = "0.9.09"

TARFILE = u-tic_$(VERSION).tar.gz

CONFFILES = config feeds passwd restricted areas felist help outaltkoi8
SRCFILES  = queuemail.c crc.c makeflock.c u-tic.pl u-fix.pl xfutil.pl \
            ftn.pl semaphore.pl misc.pl make_machdep.pl ctlfiles.pl \
            hacks.pl u-attach.pl
MISCFILES = README Makefile MAKE.CONFIG
TOOLS = 
ALLFILES = $(CONFFILES) $(SRCFILES) $(MISCFILES) $(TOOLS)
EXECFILES = crc u-tic.pl u-fix.pl xfutil.pl ftn.pl semaphore.pl\
            misc.pl machdep.pl ctlfiles.pl hacks.pl u-attach.pl queuemail

all: queuemail crc u-tic.pl u-fix.pl

install: all
	for dir in $(CONFPATH) $(EXECPATH); do; if [ ! -e $$dir ]; then; mkdir -p $$dir; fi; done
	for file in $(EXECFILES); do; $(INSTALL) -c -o $(OWNER) -g $(GROUP) $$file $(EXECPATH)/$$file; done

world: install
	for file in $(CONFFILES); do; $(INSTALL) -c -o $(OWNER) -g $(GROUP) $$file $(CONFPATH)/$$file; done

u-tic.pl: machdep.pl

u-fix.pl: machdep.pl

machdep.pl: makeflock make_machdep.pl
	./make_machdep.pl $(CONFPATH)/config $(VERSION) > machdep.pl || (rm -f machdep.pl; exit 2)

makeflock: makeflock.c
	$(CC) $(CCFLAGS) -o makeflock makeflock.c

crc: crc.c
	$(CC) $(CCFLAGS) -o crc crc.c

queuemail: queuemail.c
	$(CC) $(CCFLAGS) -o queuemail queuemail.c

clean:
	rm -f makeflock machdep.pl crc queuemail

tar: $(TARFILE)

$(TARFILE): filelist
	cd ..; $(TAR) cvf - `cat u-tic/.filelist` | gzip > u-tic/$(TARFILE)

filelist:
	rm .filelist; for file in $(ALLFILES); do; echo u-tic/$$file >> .filelist; done
