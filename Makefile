BUILD = build
SUBDIRS = libformant libportaudio

STATICLIB_PORTAUDIO = $(BUILD)/libportaudio/lib/.libs/libportaudio.a
STATICLIB_FORMANT = $(BUILD)/libformant/libformant.a

all: staticlibs

setup:
	-mkdir -p $(BUILD)
	cp -ru $(SUBDIRS) $(BUILD)

$(BUILD)/libportaudio: setup
	cd $@ && autoreconf -fi && ./configure
	$(MAKE) -C $@

$(STATICLIB_PORTAUDIO): $(BUILD)/libportaudio

$(BUILD)/libformant: setup
	$(MAKE) -C $@

$(STATICLIB_FORMANT): $(BUILD)/libformant

staticlibs: $(STATICLIB_FORMANT) $(STATICLIB_PORTAUDIO)

clean:
	-rm -r $(BUILD)

.PHONY: all setup staticlibs clean
