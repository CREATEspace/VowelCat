SUBDIRS = libformant libportaudio

subdirs: $(SUBDIRS)

libformant:
	$(MAKE) -C $@

libportaudio:
	cd $@ && autoreconf -fi && ./configure
	$(MAKE) -C $@

.PHONY: subdirs $(SUBDIRS)
