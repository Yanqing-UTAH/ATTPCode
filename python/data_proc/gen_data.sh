#!/bin/bash

if [ $# -lt 1 ]; then
    DS_LIST="small medium big"
else
    DS_LIST="$@"
fi


for DS in $DS_LIST; do

    python3 << EOF
import time_serial_matrix

time_serial_matrix.${DS}()
EOF
    mv X_${DS}.csv matrix_${DS}.csv

done
      
