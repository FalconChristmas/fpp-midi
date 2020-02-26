include /opt/fpp/src/makefiles/common/setup.mk
include /opt/fpp/src/makefiles/platform/*.mk

all: libfpp-midi.so
debug: all

CFLAGS+=-I.
OBJECTS_fpp_midi_so += src/FPPMIDI.o src/tinyexpr.o
LIBS_fpp_midi_so += -L/opt/fpp/src -lfpp -lrtmidi
CXXFLAGS_src/FPPOSC.o += -I/opt/fpp/src


%.o: %.cpp Makefile /usr/include/rtmidi/RtMidi.h
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

%.o: %.c Makefile
	$(CCACHE) gcc $(CFLAGS)  -c $< -o $@

libfpp-midi.so: $(OBJECTS_fpp_midi_so) /opt/fpp/src/libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_midi_so) $(LIBS_fpp_midi_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-midi.so $(OBJECTS_fpp_midi_so)

/usr/include/rtmidi/RtMidi.h:
	sudo ./install_librtmidi.sh
    

