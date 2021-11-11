#!/bin/bash

## requires python3, numpy, scipy, sklearn

BASEDIR="$(realpath "`dirname "$0"`")"
cd "$BASEDIR"

if [ $# -lt 1 ]; then
    DS_LIST="small medium big"
else
    DS_LIST="$@"
fi

if [ ! -x ./insert_mem_stats ]; then
    g++ -std=gnu++17 -O2 -o ./insert_mem_stats ./insert_mem_stats.cpp
fi

mkdir -p ../data
for DS in $DS_LIST; do

    python3 << EOF
import time_serial_matrix

time_serial_matrix.${DS}()
EOF
    NLINES=`cat X_${DS}.csv | wc -l`
    ./insert_mem_stats X_${DS}.csv XS_${DS}.csv $NLINES 100
    rm -f X_${DS}.csv
    cat XS_${DS}.csv mat_queries.txt > ../data/matrix_${DS}-w-stats-report.txt
    rm -f XS_${DS}.csv
done
      
