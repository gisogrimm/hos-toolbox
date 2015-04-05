ARCH = $(shell uname -m)

PREFIX = /usr/local

BINFILES = ommo_bridge hos_sphere_amb30 hos_sendosc hos_delay		\
hos_cyclephase hos_visualize hos_visualize_sphere hos_cyclephasegui	\
hos_sampler hos_scope hos_osc2jack hos_resfilt hos_tmcm			\
hos_osc_marais hos_osc_house hos_rtmdisplay hos_composer hos_rtm2midi	\
test_duration hos_foacasa hos_mm hos_markerbroadcast hos_marker2osc	\
laserctl hos_sustain hos_if_filter hos_instdc hos_spksim

#BINFILES = hos_visualize_sphere

# debug_midi

#test_jack_db

#BINFILES = hos_sustain

OBJECTS = libhos_midi_ctl.o libhos_gainmatrix.o libhos_audiochunks.o tmcm.o  libhos_random.o lininterp.o

GUIOBJ = hosgui_meter.o hosgui_mixer.o hosgui_sphere.o 



#	hos_sphere_amb30 \
#	hos_sphere_xyz \
#	mm_file \
#	mm_gui \
#	mm_midicc \
#	mm_hdsp \
#	hos_visualize \
#	hos_visualize_sphere \
#	hos_oscrmsmeter \
#	hos_marker2osc \
#	hos_markerbroadcast \
#	hos_sendosc \
#	mplayer_jack_transport \


INSTBIN = $(patsubst %,$(PREFIX)/bin/%,$(BINFILES))

JACKBIN = \
	hos_sphere_amb30 \
	hos_sphere_xyz \
	mplayer_jack_transport hos_marker2osc

LOBIN = \
	$(APP_HOS) \

ALSABIN = mm_midicc mm_hdsp

GTKMMBIN = hos_oscrmsmeter hos_visualize mm_gui hos_visualize_sphere tascar_draw hos_cyclephasegui hos_scope hos_rtmdisplay hos_foacasa hos_mm

ifeq "$(ARCH)" "x86_64"
CXXFLAGS += -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -fno-finite-math-only
endif

CXXFLAGS += -Wall -O3 -L./
#CXXFLAGS += -Wall -g -L./

EXTERNALS = alsa jack libxml++-2.6 liblo fftw3f sndfile

laserctl: EXTERNALS += libserial

LDLIBS += -ltascar
LDFLAGS += -L../tascar/build
CXXFLAGS += -I../tascar/src
CPPFLAGS += -I../tascar/src

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

all: libtascar
	mkdir -p build
	$(MAKE) -C build -f ../Makefile $(BINFILES)

libtascar:
	$(MAKE) -C tascar lib

install:
	$(MAKE) -C build -f ../Makefile $(INSTBIN)

uninstall:
	rm -f $(INSTBIN)

clean:
	$(MAKE) -C tascar clean
	rm -Rf *~ src/*~ build doc/html

VPATH = ../src ../src/hoafilt

.PHONY : doc

doc:
	cd doc && sed -e 's/PROJECT.NUMBER.*=.*/&'`cat ../version`'/1' doxygen.cfg > .temp.cfg && doxygen .temp.cfg
	rm -Rf doc/.temp.cfg

include $(wildcard *.mk)

$(PREFIX)/bin/%: %
	cp $< $@

$(BINFILES): $(OBJECTS)

%: %.o
	$(CXX) $(CXXFLAGS) $(LDLIBS) $^ -o $@

%.o: %.cc
	$(CPP) $(CPPFLAGS) -MM -MF $(@:.o=.mk) $<
	$(CXX) $(CXXFLAGS) -c $< -o $@

hos_theremin: LDLIBS += -lfftw3f

hos_theremin: EXTERNALS += gtkmm-3.0

hos_sphere_amb30: libhos_sphereparam.o 

$(GTKMMBIN): $(GUIOBJ)

$(GTKMMBIN): EXTERNALS += gtkmm-3.0

hos_composer: libhos_music.o libhos_random.o libhos_harmony.o
hos_rtmdisplay: libhos_music.o
hos_rtm2midi: libhos_music.o
test_duration: libhos_music.o

# Local Variables:
# compile-command: "make"
# End:
