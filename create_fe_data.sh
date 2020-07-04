#!/bin/bash

if [ $# -lt 1 ]; then
    echo "usage: $0 <datafile> [is_bitp = 0]"
    exit 1
fi

if [ $# -ge 2 ]; then
    IS_BITP=$2
else
    IS_BITP=0
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

if [ $IS_BITP -eq 0 ]; then
    CONF_TEMPLATE=configs/generate-fe-key-set.conf
else
    CONF_TEMPLATE=configs/generate-fe-key-set-bitp.conf
fi

CONF=`mktemp`
KEYSET_FILE=`mktemp`

cat $CONF_TEMPLATE |
sed 's,outfile =.*,outfile = '$KEYSET_FILE',' |
sed 's,infile =.*,infile = '$1',' > $CONF

./driver run $CONF
./misc/create_fe_data $1 $KEYSET_FILE

rm -f $CONF
rm -f $KEYSET_FILE
