# Export all variables to subshells
export

BUILD = build
SUBDIRS = libformant libportaudio

STATICLIB_PORTAUDIO = $(BUILD)/libportaudio.a
STATICLIB_FORMANT = $(BUILD)/libformant.a

STATICLIB_PORTAUDIO_BUILD = $(BUILD)/libportaudio/lib/.libs/libportaudio.a
STATICLIB_FORMANT_BUILD = $(BUILD)/libformant/libformant.a

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

# Common commands to set up a build dir.
define set_up_build =
    -mkdir -p $(BUILD)
    cp -ru $< $@
endef

all: staticlibs

$(BUILD)/libportaudio: libportaudio
	$(set_up_build)
	cd $@ && autoreconf -fi && ./configure --enable-static
	$(MAKE) -C $@

$(STATICLIB_PORTAUDIO_BUILD): $(BUILD)/libportaudio

$(STATICLIB_PORTAUDIO): $(STATICLIB_PORTAUDIO_BUILD)
	cp $< $@

$(BUILD)/libformant: libformant
	$(set_up_build)
	$(MAKE) -C $@

$(STATICLIB_FORMANT_BUILD): $(BUILD)/libformant

$(STATICLIB_FORMANT): $(STATICLIB_FORMANT_BUILD)
	cp $< $@

staticlibs: $(STATICLIB_FORMANT) $(STATICLIB_PORTAUDIO)

clean:
	-rm -r $(BUILD)

.PHONY: all staticlibs clean
