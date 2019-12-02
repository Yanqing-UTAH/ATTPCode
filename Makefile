CXX = ./bin/g++-less
LINK.o = $(LINK.cc)
CXXFLAGS = -Wall -O0 -g -std=c++17
CPPFLAGS =
LDFLAGS =

EXES=test_hh driver
OBJS=test_pams.o sketch.o test_hh.o conf.o exact_query.o misra_gries.o pcm.o pla.o sampling.o heavyhitters.o test_pcm.o driver.o pmmg.o test_pla.o test_conf.o pams.o 

.PHONY: all clean depend

all: $(EXES)

# exe targets

test_pla: test_pla.o pla.o conf.o

test_pcm: test_pcm.o pcm.o pla.o conf.o

test_pams: test_pams.o pams.o conf.o

test_hh: test_hh.o heavyhitters.o pcm.o pla.o conf.o

driver: driver.o pla.o pcm.o pams.o sampling.o heavyhitters.o sketch.o exact_query.o conf.o misra_gries.o pmmg.o

# objs

test_pams.o: test_pams.cpp pams.h util.h sketch.h

sketch.o: sketch.cpp sketch.h util.h pcm.h pla.h pams.h sampling.h \
 heavyhitters.h exact_query.h sketch_list.h

test_hh.o: test_hh.cpp heavyhitters.h pcm.h pla.h util.h sketch.h

conf.o: conf.cpp conf.h hashtable.h util.h config_list.h

exact_query.o: exact_query.cpp exact_query.h sketch.h util.h conf.h \
 hashtable.h

misra_gries.o: misra_gries.cpp misra_gries.h hashtable.h

pcm.o: pcm.cpp pcm.h pla.h util.h sketch.h conf.h hashtable.h

pla.o: pla.cpp pla.h

sampling.o: sampling.cpp sampling.h sketch.h util.h conf.h hashtable.h

heavyhitters.o: heavyhitters.cpp heavyhitters.h pcm.h pla.h util.h \
 sketch.h conf.h hashtable.h

test_pcm.o: test_pcm.cpp pcm.h pla.h util.h sketch.h

driver.o: driver.cpp sketch.h util.h conf.h hashtable.h misra_gries.h

pmmg.o: pmmg.cpp pmmg.h util.h conf.h hashtable.h

test_pla.o: test_pla.cpp pla.h

test_conf.o: test_conf.cpp conf.h hashtable.h

pams.o: pams.cpp pams.h util.h sketch.h conf.h hashtable.h


# end of objs
# do not remove this line

depend:
	./gen_deps.sh $(CXX) $(CXXFLAGS) $(CPPFLAGS)

clean:
	rm -f $(OBJS) $(EXES)

