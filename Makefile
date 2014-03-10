# Public make rules:
#
#  - make
#  	build libraries and link gui into build directory
#  - make -B
#  	force rebuild all libraries and gui
#  - rm -r build/libformant build/qtGui; make
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

# Public make flags:
#
#  - DEBUG=1
#       disable optimizations and enable debugging symbols
#  - OPTIMIZE=1
#       enable compiler and link-time optimizations
#  - NATIVE=1
#       compile for the native instruction set of the host computer
#  - PROFILE=1
#       enable gprof profiling symbols

# The build process is split into multiple -- currently two -- stages:
#
#  1. Populate build directory.
#  2. Build static libraries and generate their pkgconfigs.
#  3. Link binaries with static libraries.
#
# The stage to run is specified by the STAGE environmental variable. If no stage
# is given, all stages are ran.

# Variables to export to submakes.
export CFLAGS LDFLAGS QMAKEFLAGS

# Capture variables from the enviromnent.
ECFLAGS := $(CFLAGS)
override CFLAGS =
ELDFLAGS := $(LDFLAGS)
override LDFLAGS =
EQMAKEFLAGS := $(QMAKEFLAGS)
override QMAKEFLAGS =

# Relative and absolute path where things will be built.
BUILD = build
BUILD_ABS = $(PWD)/$(BUILD)

# What platform we're running on.
UNAME = $(shell uname)

# Set up variables for submodules to build -- order is important!
SRC_AUDIO = libaudio
BUILD_AUDIO = $(BUILD)/$(SRC_AUDIO)
STATICLIB_AUDIO = $(BUILD)/libaudio.a
STATICLIB_AUDIO_BUILD = $(BUILD_AUDIO)/libaudio.a
DIRS += $(SRC_AUDIO)
STATICLIBS += $(STATICLIB_AUDIO)
PKG_CONFIGS += $(BUILD_AUDIO)/libaudio.pc

SRC_PORTAUDIO = libportaudio
BUILD_PORTAUDIO = $(BUILD)/$(SRC_PORTAUDIO)
STATICLIB_PORTAUDIO = $(BUILD)/libportaudio.a
STATICLIB_PORTAUDIO_BUILD = $(BUILD_PORTAUDIO)/lib/.libs/libportaudio.a
DIRS += $(SRC_PORTAUDIO)
STATICLIBS += $(STATICLIB_PORTAUDIO)
PKG_CONFIGS += $(BUILD_PORTAUDIO)/portaudio-2.0.pc

SRC_FORMANT = libformant
BUILD_FORMANT = $(BUILD)/$(SRC_FORMANT)
STATICLIB_FORMANT = $(BUILD)/libformant.a
STATICLIB_FORMANT_BUILD = $(BUILD_FORMANT)/libformant.a
DIRS += $(SRC_FORMANT)
STATICLIBS += $(STATICLIB_FORMANT)
PKG_CONFIGS += $(BUILD_FORMANT)/libformant.pc

SRC_QTGUI = qtGui
BUILD_QTGUI = $(BUILD)/$(SRC_QTGUI)
DIRS += $(SRC_QTGUI)

ifeq ($(UNAME), Linux)
APP_QTGUI = $(BUILD_QTGUI)/qtGui
else ifeq ($(UNAME), Darwin)
APP_QTGUI = $(BUILD_QTGUI)/qtGui.app
endif

APPS += $(APP_QTGUI)

ifneq ($(STAGE), )
    override CFLAGS += -I$(BUILD_ABS)/libportaudio/include
    override CFLAGS += -I$(BUILD_ABS)/libaudio
    override CFLAGS += -I$(BUILD_ABS)/libformant
    override CFLAGS += $(ECFLAGS)

    ifeq ($(OPTIMIZE), 1)
	CFLAGS += -O2 -flto -DNDEBUG
	LDFLAGS += -O2 -flto
	QMAKEFLAGS += CONFIG+=release
    endif

    ifeq ($(NATIVE), 1)
	CFLAGS += -march=native
	LDFLAGS += -march=native
    endif

    ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g
	QMAKEFLAGS += CONFIG+=debug
    endif

    ifeq ($(PROFILE), 1)
	CFLAGS += -pg
	LDFLAGS += -pg
    endif
endif

ifeq ($(STAGE), 3)
    # Tell the compiler to search the build dir for libs.
    override LDFLAGS += -L$(BUILD_ABS)
    # Include the LDFLAGS required by libs.
    override LDFLAGS += $(shell pkg-config --libs $(PKG_CONFIGS))
    override LDFLAGS += $(ELDFLAGS)
endif

ifeq ($(STAGE), )
all: stage-3
else ifeq ($(STAGE), 1)
all: $(DIRS)
else ifeq ($(STAGE), 2)
all: $(STATICLIBS)
else ifeq ($(STAGE), 3)
all: $(APP_QTGUI)
endif

# Create the build directory.
$(BUILD):
	-mkdir -p $@

$(DIRS): | $(BUILD)
	cp -ru $@ $(BUILD)

# Build the portaudio static library.
$(STATICLIB_PORTAUDIO):
	cd $(BUILD_PORTAUDIO) && ./configure --enable-static
	$(MAKE) -C $(BUILD_PORTAUDIO)
	cp $(STATICLIB_PORTAUDIO_BUILD) $@

# Build the formant static library.
$(STATICLIB_FORMANT):
	$(MAKE) -C $(BUILD_FORMANT)
	cp $(STATICLIB_FORMANT_BUILD) $@

# Build the audio static library.
$(STATICLIB_AUDIO):
	$(MAKE) -C $(BUILD_AUDIO)
	cp $(STATICLIB_AUDIO_BUILD) $@

# Build the main app.
$(APP_QTGUI):
	cp -ru $(SRC_QTGUI) $(BUILD)
	$(MAKE) -C $(BUILD_QTGUI)

# Force these to be remade every time.
.PHONY: $(DIRS) $(STATICLIB_FORMANT) $(STATICLIB_AUDIO) $(APP_QTGUI)

stage-1:
	$(MAKE) STAGE=1

stage-2: stage-1
	$(MAKE) STAGE=2

stage-3: stage-2
	$(MAKE) STAGE=3

ifeq ($(DIR), )
compile:
	@echo error: specify a directory to compile with DIR=
else ifeq ($(STAGE), 2)
compile:
	$(MAKE) -C $(DIR)
else
compile:
	$(MAKE) STAGE=2 compile
endif

ifeq ($(DIR), )
link:
	@echo error: specify a directory to link with DIR=
else ifeq ($(STAGE), 3)
link:
	$(MAKE) -C $(DIR)
else
link: stage-1
	$(MAKE) STAGE=3 link
endif

ifeq ($(UNAME), Darwin)
deploy: $(APP_QTGUI)
	macdeployqt $<
else
deploy: $(APP_QTGUI)
endif

clean:
	-rm -r $(BUILD)

.PHONY: all stage-1 stage-2 stage-3 compile link deploy clean
