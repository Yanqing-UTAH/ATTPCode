## Dependencies
lapacke, cblas, fftw3

## How to compile:
1. To configure it with -O2 -DNDEBUG flags, run

    ./configure
    
or to configure it with -O0 -g flags, run

    ./configure --enable-debug 

2. To compile, run

    make

3. If you added/removed some files and/or added headers to some cpp, call the following first before compiling it

    make depend


Note: avl.h, min_heap.h and basic_defs.h are hard links on ravenserv1.
