CXX = g++
LINK.o = $(LINK.cc)
CXXFLAGS = -Wall -O0 -g -std=c++17
CPPFLAGS =
LDFLAGS =

EXES=test_hh driver
OBJS=test_pams.o sketch.o test_hh.o pcm.o pla.o sampling.o heavyhitters.o test_pcm.o driver.o test_pla.o pams.o 

.PHONY: all clean depend

all: $(EXES)

# exe targets

test_pla: test_pla.o pla.o

test_pcm: test_pcm.o pcm.o pla.o

test_pams: test_pams.o pams.o

test_hh: test_hh.o heavyhitters.o pcm.o pla.o

driver: driver.o pla.o pcm.o pams.o sampling.o heavyhitters.o sketch.o

# objs

test_pams.o: test_pams.cpp pams.h util.h sketch.h

sketch.o: sketch.cpp sketch.h util.h pcm.h pla.h pams.h sampling.h \
 sketch_list.h

test_hh.o: test_hh.cpp heavyhitters.h pcm.h pla.h util.h sketch.h

pcm.o: pcm.cpp pcm.h pla.h util.h sketch.h

pla.o: pla.cpp pla.h

sampling.o: sampling.cpp sampling.h sketch.h util.h

heavyhitters.o: heavyhitters.cpp heavyhitters.h pcm.h pla.h util.h \
 sketch.h

test_pcm.o: test_pcm.cpp pcm.h pla.h util.h sketch.h

driver.o: driver.cpp sketch.h util.h

test_pla.o: test_pla.cpp pla.h

pams.o: pams.cpp pams.h util.h sketch.h


# end of objs
# do not remove this line

depend:
	./gen_deps.sh $(CXX) $(CXXFLAGS) $(CPPFLAGS)

clean:
	rm -f $(OBJS) $(EXES)

