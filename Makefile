CC = COMPILER='gcc' ./bin/compile
CXX = COMPILER='g++ -std=gnu++17' ./bin/compile
LINK.o = $(LINK.cc)
CXXFLAGS =  -O2 -Wall -Wno-comment
CPPFLAGS =  -DNDEBUG -Iexternal/fftw-3.3.8/api
LDFLAGS = 
LDLIBS = -llapacke -llapack -lcblas  -pthread

# TODO honor top_srcdir across all rules
top_srcdir = .

EXES=driver
OBJS=test_pla.o driver.o sketch.o old_driver.o test_conf.o misra_gries.o test_hh.o norm_sampling.o fd.o pmmg.o perf_timer.o pcm.o test_pams.o pams.o lapack_wrapper.o exact_query.o MurmurHash3.o query.o conf.o sampling.o pla.o test_dct.o heavyhitters.o test_pcm.o 
DRIVER_OBJS=driver.o sketch.o old_driver.o misra_gries.o norm_sampling.o fd.o pmmg.o perf_timer.o pcm.o pams.o lapack_wrapper.o exact_query.o MurmurHash3.o query.o conf.o sampling.o pla.o heavyhitters.o 

FFTW3_BUILD_DIR=external/fftw-3.3.8/build
FFTW3_LDFLAGS=-L$(FFTW3_BUILD_DIR)/lib
FFTW3_LIBS=-lfftw3

.PHONY: all clean depend driver_tgt fftw3

all: driver_tgt

# exe targets

test_pla: test_pla.o pla.o conf.o MurmurHash3.o

test_pcm: test_pcm.o pcm.o pla.o conf.o MurmurHash3.o

test_pams: test_pams.o pams.o conf.o MurmurHash3.o

test_hh: test_hh.o heavyhitters.o pcm.o pla.o conf.o MurmurHash3.o

test_dct: fftw3 test_dct.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o test_dct test_dct.cpp $(LDFLAGS) $(FFTW3_LDFLAGS) $(LDLIBS) $(FFTW3_LIBS)

driver_tgt: fftw3 driver

driver: $(DRIVER_OBJS)
	$(CXX) -o driver $(DRIVER_OBJS) $(LDFLAGS) $(FFTW3_LDFLAGS) $(LDLIBS) $(FFTW3_LIBS)

# libs
fftw3:
	cd external/fftw-3.3.8; if [ '(' ! -e build/include/fftw3.h ')' -o '(' ! -e build/lib/libfftw3.a ')' ]; then ./configure --prefix="`pwd`/build" && make && make install; fi

# objs

test_pla.o: test_pla.cpp pla.h

driver.o: driver.cpp conf.h hashtable.h misra_gries.h sketch.h util.h \
 sketch_lib.h query.h

sketch.o: sketch.cpp sketch.h util.h sketch_lib.h pcm.h pla.h \
 MurmurHash3.h pams.h sampling.h avl.h basic_defs.h avl_container.h \
 heavyhitters.h exact_query.h pmmg.h misra_gries.h hashtable.h min_heap.h \
 dummy_persistent_misra_gries.h conf.h norm_sampling.h fenwick_tree.h \
 fd.h sketch_list.h

old_driver.o: old_driver.cpp sketch.h util.h sketch_lib.h

test_conf.o: test_conf.cpp conf.h hashtable.h

misra_gries.o: misra_gries.cpp misra_gries.h hashtable.h sketch.h util.h \
 sketch_lib.h

test_hh.o: test_hh.cpp heavyhitters.h pcm.h pla.h util.h sketch.h \
 sketch_lib.h MurmurHash3.h

norm_sampling.o: norm_sampling.cpp norm_sampling.h fenwick_tree.h \
 sketch.h util.h sketch_lib.h min_heap.h basic_defs.h conf.h hashtable.h

fd.o: fd.cpp fd.h sketch.h util.h sketch_lib.h conf.h hashtable.h

pmmg.o: pmmg.cpp pmmg.h util.h misra_gries.h hashtable.h sketch.h \
 sketch_lib.h min_heap.h basic_defs.h conf.h

perf_timer.o: perf_timer.cpp perf_timer.h

pcm.o: pcm.cpp pcm.h pla.h util.h sketch.h sketch_lib.h MurmurHash3.h \
 conf.h hashtable.h

test_pams.o: test_pams.cpp pams.h util.h sketch.h sketch_lib.h \
 MurmurHash3.h

pams.o: pams.cpp pams.h util.h sketch.h sketch_lib.h MurmurHash3.h conf.h \
 hashtable.h

lapack_wrapper.o: lapack_wrapper.c

exact_query.o: exact_query.cpp exact_query.h sketch.h util.h sketch_lib.h \
 conf.h hashtable.h

MurmurHash3.o: MurmurHash3.cpp MurmurHash3.h

query.o: query.cpp util.h conf.h hashtable.h sketch.h sketch_lib.h \
 perf_timer.h lapack_wrapper.h

conf.o: conf.cpp conf.h hashtable.h util.h config_list.h

sampling.o: sampling.cpp sampling.h sketch.h util.h sketch_lib.h avl.h \
 basic_defs.h avl_container.h conf.h hashtable.h min_heap.h

pla.o: pla.cpp pla.h

test_dct.o: test_dct.cpp

heavyhitters.o: heavyhitters.cpp heavyhitters.h pcm.h pla.h util.h \
 sketch.h sketch_lib.h MurmurHash3.h conf.h hashtable.h

test_pcm.o: test_pcm.cpp pcm.h pla.h util.h sketch.h sketch_lib.h \
 MurmurHash3.h


# end of objs
# do not remove this line

depend:
	CXX='g++ -std=gnu++17' CC='gcc' ./gen_deps.sh ./bin/compile $(CXXFLAGS) $(CPPFLAGS)

clean:
	rm -f $(OBJS) $(EXES)

# below is for auto remaking configure and config.h

$(top_srcdir)/configure: $(top_srcdir)/configure.ac $(top_srcdir)/aclocal.m4
	cd '$(top_srcdir)' && autoconf

$(top_srcdir)/config.h.in: $(top_srcdir)/stamp-h.in

$(top_srcdir)/stamp-h.in: $(top_srcdir)/configure.ac $(top_srcdir)/aclocal.m4
	cd '$(top_srcdir)' && autoheader && echo timestamp > stamp-h.in && rm -f 'config.h.in~'

config.h: stamp-h
stamp-h: $(top_srcdir)/config.h.in config.status
	./config.status

Makefile: $(top_srcdir)/Makefile.in config.status
	./config.status

config.status: $(top_srcdir)/configure $(top_srcdir)/config.h.in
	./config.status --recheck


