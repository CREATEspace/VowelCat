# Public make rules:
#
#  - make
#  	build libraries and link gui into build directory
#  - make -B
#  	force rebuild all libraries and gui
#  - rm build/libformant build/qtGui; make
#  	force rebuild/relink of libformant and the gui
#  - make deploy
#  	on mac, package binary into standalone app
#  - make compile DIR=scratch/dir
#  	compile (but don't link) code in scratch/dir using the Makefile in that
#  	directory
#  - make link DIR=qtGui
#  	link the code in qtGui using the Makefile in that directory
#  - make clean
#  	remove build directory

# The build process is split into multiple -- currently two -- stages:
#
#  1. Build static libraries and generate their pkgconfigs.
#  2. Link binaries with static libraries.
#
# The stage to run is specified by the STAGE environmental variable. If no stage
# is given, all stages are ran.

# Export CFLAGS and LDFLAGS to submakes.
export CFLAGS LDFLAGS

# Relative and absolute path where things will be built.
BUILD = build
BUILD_ABS = $(PWD)/$(BUILD)

# What platform we're running on.
UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
QTGUI = $(BUILD)/qtGui/qtGui
else ifeq ($(UNAME), Darwin)
QTGUI = $(BUILD)/qtGui/qtGui.app
endif

# Set up apps to build
APPS += $(QTGUI)

# Build directories.
BUILD_PORTAUDIO = $(BUILD)/libportaudio
BUILD_FORMANT = $(BUILD)/libformant
BUILD_AUDIO = $(BUILD)/libaudio

# Final paths of static libraries.
STATICLIB_PORTAUDIO = $(BUILD)/libportaudio.a
STATICLIB_FORMANT = $(BUILD)/libformant.a
STATICLIB_AUDIO = $(BUILD)/libaudio.a

# Collect all the static libs here.
STATICLIBS = \
    $(STATICLIB_PORTAUDIO) \
    $(STATICLIB_FORMANT) \
    $(STATICLIB_AUDIO) \

# Source paths of static libraries.
STATICLIB_PORTAUDIO_BUILD = $(BUILD_PORTAUDIO)/lib/.libs/libportaudio.a
STATICLIB_FORMANT_BUILD = $(BUILD_FORMANT)/libformant.a
STATICLIB_AUDIO_BUILD = $(BUILD_AUDIO)/libaudio.a

# Pkg-config files to use.
PKG_CONFIGS = \
    $(BUILD_AUDIO)/libaudio.pc \
    $(BUILD_PORTAUDIO)/portaudio-2.0.pc \
    $(BUILD_FORMANT)/libformant.pc \

# If on any stage, set up CFLAGS.
ifneq ($(STAGE), )
    CFLAGS += -I$(BUILD_ABS)/libportaudio/include
    CFLAGS += -I$(BUILD_ABS)/libaudio
    CFLAGS += -I$(BUILD_ABS)/libformant
endif

# If on stage 2, set up LDFLAGS.
ifeq ($(STAGE), 2)
    # Tell the compiler to search the build dir for libs.
    LDFLAGS += -L$(BUILD_ABS)
    # Include the LDFLAGS required by libs.
    LDFLAGS += $(shell pkg-config --libs $(PKG_CONFIGS))
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
all: stage-2
else ifeq ($(STAGE), 1)
all: $(STATICLIBS)
else ifeq ($(STAGE), 2)
all: $(APPS)
endif

# Create the build directory.
$(BUILD):
	-mkdir -p $@

# Build the portaudio static library.
$(BUILD)/libportaudio: libportaudio | $(BUILD)
	cp -ru $< $|

$(STATICLIB_PORTAUDIO_BUILD): | $(BUILD)/libportaudio
	cd $| && autoreconf -fi && ./configure --enable-static
	$(MAKE) -C $|

$(STATICLIB_PORTAUDIO): $(STATICLIB_PORTAUDIO_BUILD)
	cp $< $@

# Build the formant static library.
$(BUILD)/libformant: libformant | $(BUILD)
	cp -ru $< $|

$(STATICLIB_FORMANT_BUILD): $(BUILD)/libformant
	$(MAKE) -C $<

$(STATICLIB_FORMANT): $(STATICLIB_FORMANT_BUILD)
	cp $< $@

# Build the audio static library.
$(BUILD)/libaudio: libaudio | $(BUILD)
	cp -ru $< $|

$(STATICLIB_AUDIO_BUILD): $(BUILD)/libaudio
	$(MAKE) -C $<

$(STATICLIB_AUDIO): $(STATICLIB_AUDIO_BUILD)
	cp $< $@

# Build the main app.
$(BUILD)/qtGui: qtGui | $(BUILD)
	cp -ru $< $|

$(QTGUI): $(BUILD)/qtGui
	$(MAKE) -C $<

stage-1:
	$(MAKE) STAGE=1

stage-2: stage-1
	$(MAKE) STAGE=2

ifeq ($(DIR), )
compile:
	@echo error: specify a directory to compile with DIR=
else
compile:
	$(MAKE) -C $(DIR)
endif

# Build linked binaries.
ifeq ($(DIR), )
link:
	@echo error: specify a directory to link with DIR=
else ifeq ($(STAGE), 2)
link:
	$(MAKE) -C $(DIR)
else
link: stage-1
	$(MAKE) STAGE=2 link
endif

ifeq ($(UNAME), Darwin)
deploy: $(QTGUI)
	macdeployqt $<
else
deploy: $(QTGUI)
endif

clean:
	-rm -r $(BUILD)

.PHONY: all stage-1 stage-2 compile link deploy clean
