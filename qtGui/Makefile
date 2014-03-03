QMAKE = qmake

all: Makefile.qt
	$(MAKE) -f $<

Makefile.qt: qtGui.pro
	$(QMAKE) $(QMAKEFLAGS) -o $@

clean: Makefile.qt
	$(MAKE) -f $< clean
	$(RM) $<

.PHONY: all clean
