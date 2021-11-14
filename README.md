# Codebase of At-the-time and Back-in-time persistent sketches


## Dependencies
c/c++ compileres: must be gcc/g++ >= 8. libraries: lapacke, cblas, fftw3

curl, python3, sklearn, scipy, numpy for preparing datasets.

matplotlib, python2, jupyter notebook for plotting figures.

## How to compile
1. To configure it with -O2 -DNDEBUG flags, run

    ./configure
    
or to configure it with -O0 -g flags, run

    ./configure --enable-debug 

2. To compile, run

    make

3. If you added/removed some files and/or added headers to some cpp, call the following first before compiling it

    make depend

## Usage
Run the following for help.

    ./driver

## Contact

Authors: Benwei Shi, Zhuoyue Zhao, Yanqing Peng, Feifei Li, Jeff Phillips

If you have any question about the code, please contact Zhuoyue (zzhao35 [at]
buffalo [dot] edu) for help.


