SRCDIR ?= /opt/fpp/src
include $(SRCDIR)/makefiles/common/setup.mk
include $(SRCDIR)/makefiles/platform/*.mk

all: libfpp-midi.$(SHLIB_EXT)
debug: all

CFLAGS+=-I.
OBJECTS_fpp_midi_so += src/FPPMIDI.o
LIBS_fpp_midi_so += -L$(SRCDIR) -lfpp -ljsoncpp -ldrogon -ltrantor -lrtmidi
CXXFLAGS_src/FPPMIDI.o += -I$(SRCDIR)


ifeq '$(ARCH)' 'OSX'
MIDIHEADER=$(HOMEBREW)/include/rtmidi/RtMidi.h
$(MIDIHEADER):
	brew install rtmidi
else
MIDIHEADER=/usr/include/rtmidi/RtMidi.h
$(MIDIHEADER):
	./install_librtmidi.sh
endif

%.o: %.cpp Makefile $(MIDIHEADER)
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

%.o: %.c Makefile
	$(CCACHE) $(CCOMPILER) $(CFLAGS)  -c $< -o $@

libfpp-midi.$(SHLIB_EXT): $(OBJECTS_fpp_midi_so) $(SRCDIR)/libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_midi_so) $(LIBS_fpp_midi_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-midi.so $(OBJECTS_fpp_midi_so)


