# Copyright (c) Eugene G. Crosser, 1993-1997
# Top-level makefile for ifmail package

include CONFIG

TARFILE = ifmail-${VERSION}.tar.gz
DIRS = iflib ifgate ifcico

all install depend:
	for d in ${DIRS}; do (cd $$d && echo $$d && ${MAKE} $@) || exit; done;

clean:
	rm -f .filelist ${TARFILE}
	for d in ${DIRS}; do (cd $$d && echo $$d && ${MAKE} $@) || exit; done;


tar:	${TARFILE}

${TARFILE}:	filelist
	cd ..; ${TAR} cvTf ifmail/.filelist - | gzip >ifmail/${TARFILE}

filelist:
	echo ifmail/Makefile >.filelist
	echo ifmail/CONFIG >>.filelist
	echo ifmail/README >>.filelist
	echo ifmail/LSM >>.filelist
	echo ifmail/misc >>.filelist
	for d in ${DIRS}; do (cd $$d && echo $$d && ${MAKE} filelist && \
				cat filelist >>../.filelist) || exit; done;
