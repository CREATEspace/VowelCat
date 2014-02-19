# The build process is split into multiple -- currently two -- stages:
#
#  1. Build static libraries and generate their pkgconfigs.
#  2. Link binaries with static libraries.
#
# The stage to run is specified by the STAGE environmental variable. If no stage
# is given, all stages are ran.

# Export all variables to subshells so submakes pick up the CFLAGS and LDFLAGS
# assignments below.
export CFLAGS LDFLAGS

# Relative and absolute path where things will be built.
BUILD = build
BUILD_ABS = $(PWD)/$(BUILD)

# Build directories.
BUILD_FORMANT = $(BUILD)/libformant
BUILD_PORTAUDIO = $(BUILD)/libportaudio
BUILD_AUDIO = $(BUILD)/libaudio

# Final paths of static libraries.
STATICLIB_PORTAUDIO = $(BUILD)/libportaudio.a
STATICLIB_FORMANT = $(BUILD)/libformant.a
STATICLIB_AUDIO = $(BUILD)/libaudio.a

# Collect all the static libs here.
STATICLIBS = $(STATICLIB_FORMANT) \
             $(STATICLIB_PORTAUDIO) \
             $(STATICLIB_AUDIO) \

# Source paths of static libraries.
STATICLIB_PORTAUDIO_BUILD = $(BUILD_PORTAUDIO)/lib/.libs/libportaudio.a
STATICLIB_FORMANT_BUILD = $(BUILD_FORMANT)/libformant.a
STATICLIB_AUDIO_BUILD = $(BUILD_AUDIO)/libaudio.a

# Pkg-config files to use.
PKG_CONFIGS = $(BUILD_FORMANT)/libformant.pc \
              $(BUILD_PORTAUDIO)/portaudio-2.0.pc \

ifneq ($(STAGE), )
    # Set up include dirs.
    CFLAGS += -I$(BUILD_ABS)/libportaudio/include
    CFLAGS += -I$(BUILD_ABS)/libaudio
    CFLAGS += -I$(BUILD_ABS)/libformant
endif

ifeq ($(STAGE), 2)
    # Tell the compiler to search the build dir for libs.
    LDFLAGS += -L$(BUILD_ABS)
    # Include the LDFLAGS required by libs.
    LDFLAGS += $(shell pkg-config --libs $PKG_CONFIGS)
endif

# Turn on optimizations and LTO?
ifeq ($(OPTIMIZE), 1)
    CFLAGS += -O2 -flto
    LDFLAGS += -flto
endif

# Compile for the build computer?
ifeq ($(NATIVE), 1)
    CFLAGS += -march=native
    LDFLAGS += -march=native
endif

# Enable debug symbols?
ifeq ($(DEBUG), 1)
    CFLAGS += -O0 -g
endif

ifeq ($(STAGE), )
all: bootstrap
endif

ifeq ($(STAGE), 1)
all: staticlibs
endif

ifeq ($(STAGE), 2)
all: link
endif

# Build both stages.
bootstrap:
	$(MAKE) STAGE=1
	$(MAKE) STAGE=2

# Build static libs.
staticlibs: $(BUILD) $(STATICLIBS)

# Build linked binaries.
link:

# Create the build directory.
$(BUILD):
	-mkdir -p $@

# Build the portaudio static library.
$(BUILD)/libportaudio: libportaudio
	cp -ru $< $(BUILD)
	cd $@ && autoreconf -fi && ./configure --enable-static
	$(MAKE) -C $@

$(STATICLIB_PORTAUDIO_BUILD): $(BUILD)/libportaudio

$(STATICLIB_PORTAUDIO): $(STATICLIB_PORTAUDIO_BUILD)
	cp $< $@

# Build the formant static library.
$(BUILD)/libformant: libformant
	cp -ru $< $(BUILD)
	$(MAKE) -C $@

$(STATICLIB_FORMANT_BUILD): $(BUILD)/libformant

$(STATICLIB_FORMANT): $(STATICLIB_FORMANT_BUILD)
	cp $< $@

# Build the audio static library.
$(BUILD)/libaudio: libaudio
	cp -ru $< $(BUILD)
	$(MAKE) -C $@

$(STATICLIB_AUDIO_BUILD): $(BUILD)/libaudio

$(STATICLIB_AUDIO): $(STATICLIB_AUDIO_BUILD)
	cp $< $@

clean:
	-rm -r $(BUILD)

.PHONY: all boostrap staticlibs link clean
