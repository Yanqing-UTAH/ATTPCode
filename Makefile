CXX = ./bin/g++-less
LINK.o = $(LINK.cc)
#CXXFLAGS = -Wall -O0 -g -std=c++17
CXXFLAGS = -Wall -O2 -std=c++17
CPPFLAGS =
LDFLAGS =

EXES=test_hh driver
OBJS=test_pams.o sketch.o test_hh.o conf.o old_driver.o exact_query.o misra_gries.o pcm.o pla.o sampling.o heavyhitters.o test_pcm.o driver.o pmmg.o test_pla.o test_conf.o MurmurHash3.o perf_timer.o pams.o 

.PHONY: all clean depend

all: $(EXES)

# exe targets

test_pla: test_pla.o pla.o conf.o MurmurHash3.o

test_pcm: test_pcm.o pcm.o pla.o conf.o MurmurHash3.o

test_pams: test_pams.o pams.o conf.o MurmurHash3.o

test_hh: test_hh.o heavyhitters.o pcm.o pla.o conf.o MurmurHash3.o

driver: driver.o pla.o pcm.o pams.o sampling.o heavyhitters.o sketch.o exact_query.o conf.o misra_gries.o pmmg.o MurmurHash3.o old_driver.cpp perf_timer.o

# objs

test_pams.o: test_pams.cpp pams.h util.h sketch.h MurmurHash3.h

sketch.o: sketch.cpp sketch.h util.h pcm.h pla.h MurmurHash3.h pams.h \
 sampling.h heavyhitters.h exact_query.h pmmg.h misra_gries.h hashtable.h \
 dummy_persistent_misra_gries.h conf.h sketch_list.h

test_hh.o: test_hh.cpp heavyhitters.h pcm.h pla.h util.h sketch.h \
 MurmurHash3.h

conf.o: conf.cpp conf.h hashtable.h util.h config_list.h

old_driver.o: old_driver.cpp sketch.h util.h

exact_query.o: exact_query.cpp exact_query.h sketch.h util.h conf.h \
 hashtable.h

misra_gries.o: misra_gries.cpp misra_gries.h hashtable.h sketch.h util.h

pcm.o: pcm.cpp pcm.h pla.h util.h sketch.h MurmurHash3.h conf.h \
 hashtable.h

pla.o: pla.cpp pla.h

sampling.o: sampling.cpp sampling.h sketch.h util.h conf.h hashtable.h

heavyhitters.o: heavyhitters.cpp heavyhitters.h pcm.h pla.h util.h \
 sketch.h MurmurHash3.h conf.h hashtable.h

test_pcm.o: test_pcm.cpp pcm.h pla.h util.h sketch.h MurmurHash3.h

driver.o: driver.cpp sketch.h util.h conf.h hashtable.h misra_gries.h \
 perf_timer.h

pmmg.o: pmmg.cpp pmmg.h util.h misra_gries.h hashtable.h sketch.h conf.h

test_pla.o: test_pla.cpp pla.h

test_conf.o: test_conf.cpp conf.h hashtable.h

MurmurHash3.o: MurmurHash3.cpp MurmurHash3.h

perf_timer.o: perf_timer.cpp perf_timer.h

pams.o: pams.cpp pams.h util.h sketch.h MurmurHash3.h conf.h hashtable.h


# end of objs
# do not remove this line

depend:
	./gen_deps.sh $(CXX) $(CXXFLAGS) $(CPPFLAGS)

clean:
	rm -f $(OBJS) $(EXES)

