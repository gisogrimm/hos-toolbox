ARCH = $(shell uname -m)

VERSION=0.1
export FULLVERSION:=$(shell ./get_version.sh)

BINFILES = hos_cyclephase hos_cyclephasegui hos_sampler hos_osc2jack	\
	 hos_resfilt hos_rtmdisplay hos_composer hos_rtm2midi		\
	 hos_foacasa hos_version

BUILDBIN = $(patsubst %,build/%,$(BINFILES))

OBJECTS = libhos_midi_ctl.o libhos_gainmatrix.o libhos_audiochunks.o tmcm.o  libhos_random.o lininterp.o

BUILDOBJ = $(patsubst %,build/%,$(OBJECTS))

GUIOBJ = hosgui_meter.o hosgui_mixer.o hosgui_sphere.o 

GTKMMBIN = hos_rtmdisplay hos_cyclephasegui hos_foacasa
#ALSABIN = mm_midicc mm_hdsp

CXXFLAGS += -std=c++11 -fext-numeric-literals
CXXFLAGS += -DHOSVERSION="\"$(FULLVERSION)\""

ifeq "$(ARCH)" "x86_64"
CXXFLAGS += -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -fno-finite-math-only
endif

CXXFLAGS += -Wall -O3 -L./

CXXFLAGS += -Wno-deprecated

EXTERNALS = alsa jack libxml++-2.6 liblo fftw3f sndfile xerces-c

laserctl: EXTERNALS += libserial
hos_cycledriver: EXTERNALS += libserial

LDLIBS += -ltascar -ldl

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

all: $(BUILDBIN)

clean:
	rm -Rf *~ src/*~ build doc/html .*version

.PHONY : doc

doc:
	cd doc && sed -e 's/PROJECT.NUMBER.*=.*/&'`cat ../version`'/1' doxygen.cfg > .temp.cfg && doxygen .temp.cfg
	rm -Rf doc/.temp.cfg

include $(wildcard build/*.mk)

$(PREFIX)/bin/%: %
	cp $< $@

$(BUILDBIN): $(BUILDOBJ)

$(patsubst %,build/%,$(GTKMMBIN)): $(patsubst %,build/%,$(GUIOBJ))

$(patsubst %,build/%,$(GTKMMBIN)): EXTERNALS += gtkmm-3.0

build/%: build/%.o
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

build/%.o: src/%.cc
	-mkdir -p build
	$(CPP) $(CPPFLAGS) -MM -MF $(@:.o=.mk) $<
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/hos_theremin: LDLIBS += -lfftw3f

build/hos_theremin: EXTERNALS += gtkmm-3.0

#build/hos_sphere_amb30: build/libhos_sphereparam.o 
#
build/hos_composer: build/libhos_music.o build/libhos_random.o build/libhos_harmony.o
build/hos_rtmdisplay: build/libhos_music.o
build/hos_rtm2midi: build/libhos_music.o
#build/test_duration: build/libhos_music.o

clangformat:
	clang-format-9 -i $(wildcard src/*.cc) $(wildcard src/*.h)

pack:
	$(MAKE) -C packaging/deb pack

# Local Variables:
# compile-command: "make"
# End:
