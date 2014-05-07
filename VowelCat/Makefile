# Copyright 2014 Formant Industries. See the Copying file at the top-level
# directory of this project.

export CFLAGS LDFLAGS

QMAKE = qmake

all: Makefile.qt
	$(MAKE) -f $<

Makefile.qt: VowelCat.pro
	$(QMAKE) $(QMAKEFLAGS) -o $@

clean: Makefile.qt
	$(MAKE) -f $< clean
	$(RM) $<

distclean: Makefile.qt
	$(MAKE) -f $< distclean

.PHONY: all clean distclean
