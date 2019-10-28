CXXFLAGS = -Wall -O0 -g -std=c++11

.PHONY: all clean

#detection: detection.cpp psketch.o pla.o
#	g++ $(CXXFLAGS) detection.cpp psketch.o pla.o -o detection
#psketch.o: psketch.cpp psketch.h
#	g++ -c $(CXXFLAGS) -o psketch.o -c psketch.cpp

all: test_pla test_pcm test_pams

test_pla: test_pla.cpp pla.o

pla.o: pla.cpp pla.h util.h

test_pcm: test_pcm.cpp pcm.o pla.o

pcm.o: pcm.cpp pcm.h pla.h util.h

test_pams: test_pams.cpp pams.o

pams.o: pams.cpp pams.h util.h

clean:
	rm -f *.o test_pla test_pams test_pcm

