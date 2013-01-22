PREFIX = /usr/local

BINFILES = ommo_bridge


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


OBJECTS = jackclient.o osc_helper.o libhos_midi_ctl.o 


INSTBIN = $(patsubst %,$(PREFIX)/bin/%,$(BINFILES))

JACKBIN = \
	hos_sphere_amb30 \
	hos_sphere_xyz \
	mplayer_jack_transport hos_marker2osc

LOBIN = \
	$(APP_HOS) \

ALSABIN = mm_midicc mm_hdsp

GTKMMBIN = hos_oscrmsmeter hos_visualize mm_gui hos_visualize_sphere hosgui_meter.o hosgui_mixer.o hosgui_sphere.o tascar_draw

CXXFLAGS += -Wall -O3 -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -fno-finite-math-only -L./

EXTERNALS = alsa jack libxml++-2.6 liblo

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

all:
	mkdir -p build
	$(MAKE) -C build -f ../Makefile $(BINFILES)

install:
	$(MAKE) -C build -f ../Makefile $(INSTBIN)

uninstall:
	rm -f $(INSTBIN)

clean:
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


# Local Variables:
# compile-command: "make"
# End: