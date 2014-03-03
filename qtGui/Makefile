QMAKE = qmake

all: Makefile.qt
	$(MAKE) -f $<

Makefile.qt: qtGui.pro
	$(QMAKE) -o $@

clean: Makefile.qt
	$(MAKE) -f $< clean
	$(RM) $<

.PHONY: all clean
