#!/bin/bash

if [ $# -lt 1 ]; then
    echo "usage: $0 <datafile>"
    exit 1
fi

if [ ! -x misc/create_fe_data ]; then
    g++ -std=gnu++17 -o misc/create_fe_data -O2 misc/create_fe_data.cpp
fi

if [ ! -d output ]; then
    mkdir output
fi

if [ ! -x ./driver ]; then
    ./configure
    make
fi

CONF=`mktemp`
KEYSET_FILE=`mktemp`
cat configs/generate-fe-key-set.conf |
sed 's,outfile =.*,outfile = '$KEYSET_FILE',' |
sed 's,infile =.*,infile = '$1',' > $CONF

./driver run $CONF
./misc/create_fe_data $1 $KEYSET_FILE

rm -f $CONF
rm -f $KEYSET_FILE
