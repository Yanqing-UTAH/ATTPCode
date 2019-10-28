CXX = g++
LINK.o = $(LINK.cc)
CXXFLAGS = -Wall -O0 -g -std=c++11
CPPFLAGS =
LDFLAGS =

EXES=test_pla test_pcm test_pams #test_point_query
OBJS=pams.o pcm.o pla.o test_pams.o test_pcm.o test_pla.o 

.PHONY: all clean depend

all: EXES

# exe targets

test_pla: test_pla.o pla.o

test_pcm: test_pcm.o pcm.o pla.o

test_pams: test_pams.o pams.o

#test_point_query: test_point_query.o pla.o pcm.o

# objs

pams.o: pams.cpp pams.h util.h

pcm.o: pcm.cpp pcm.h pla.h util.h sketch.h

pla.o: pla.cpp pla.h

test_pams.o: test_pams.cpp pams.h util.h

test_pcm.o: test_pcm.cpp pcm.h pla.h util.h sketch.h

test_pla.o: test_pla.cpp pla.h


# end of objs
# do not remove this line

depend:
	./gen_deps.sh $(CXX) $(CXXFLAGS) $(CPPFLAGS)

clean:
	rm -f $(OBJS) $(EXES)

