# Export all variables to subshells
export

BUILD = build
SUBDIRS = libformant libportaudio

STATICLIB_PORTAUDIO = $(BUILD)/libportaudio/lib/.libs/libportaudio.a
STATICLIB_FORMANT = $(BUILD)/libformant/libformant.a

# Turn on optimizations and LTO?
ifdef OPTIMIZE
    CFLAGS += -O2 -flto
    LDFLAGS += -flto
endif

# Compile for the build computer?
ifdef NATIVE
    CFLAGS += -march=native
    LDFLAGS += -march=native
endif

# Enable debug symbols?
ifdef DEBUG
    CFLAGS += -O0 -g
endif

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
