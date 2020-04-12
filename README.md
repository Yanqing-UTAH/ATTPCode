##How to compile:
1. To configure it with -O2 -DNDEBUG flags
    ./configure
or to configure it with -O0 -g flags
    ./configure --enable-debug 

2. To compile,
    make

3. If you added/removed some files and/or added headers to some cpp, call
    make depend

