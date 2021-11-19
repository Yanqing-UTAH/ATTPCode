#!/bin/bash

if [ $# -lt 1 ]; then
    echo "usage: $0 <datafile> [is_bitp = 0]"
    exit 1
fi

INFILE="$(realpath "$1")"
BASEDIR="$(realpath "`dirname "$0"`")"
cd "$BASEDIR"
cd ..

CXX=${CXX:-g++}

if [ $# -ge 2 ]; then
    IS_BITP=$2
else
    IS_BITP=0
fi

if [ ! -x ./data_proc/create_fe_data ]; then
    ${CXX} -std=gnu++17 -o ./data_proc/create_fe_data -O2 ./data_proc/create_fe_data.cpp
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
sed 's,infile =.*,infile = '$INFILE',' > $CONF

./driver run $CONF
./data_proc/create_fe_data $INFILE $KEYSET_FILE

rm -f $CONF
rm -f $KEYSET_FILE
