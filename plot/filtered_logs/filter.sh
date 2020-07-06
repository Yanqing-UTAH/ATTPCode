#!/bin/bash

if [ $# -lt 2 ]; then
    echo "usage: $0 <list_file> <outdir>"
    exit 1
fi

LIST_FILE=$1
OUT_DIR=$2

if [ ! -d $OUT_DIR ]; then
    mkdir -p $OUT_DIR
fi

LIST_OF_EXCLUDED_QUERIES=\
"HH(0.0002|10001)\|"\
"HH(0.0002|901500000)\|"\
"HH(0.01|893973655)\|"\
"HH(0.01|901500000)\|"\
"HH(0.01|0)\|"\
"HH(0.01|901489529)\|"\
"MS(10)\|"\
"MS(100)\|"\
"HH(0.0002|901489153)"

run_filter() {
    if [ -z "$PREV_INFILE" ]; then
        return 0
    fi
    
    echo "${PREV_INFILE}: $TEST_NAMES"
    NTESTS=$(echo "$TEST_NAMES" | awk '{ print NF; }')
    OUTFILE=$OUT_DIR/$PREV_INFILE

    grep 'Stats request at line\|timers\|'"$(echo "$TEST_NAMES" | sed 's/ /\\|/g' | \
        sed 's/[.]/[.]/g')"'\|^MS\|^HH' ../raw_logs/$PREV_INFILE |
    sed "/${LIST_OF_EXCLUDED_QUERIES}/{$(yes N\; | head -n $NTESTS | tr '\n' ' ')d}" > $OUTFILE

    NLINES=`wc -l $OUTFILE | awk '{ print $1; }'`
    sed -i "$(echo "" | awk "{ print $NLINES - ($NTESTS "'*'" 3 + 2) + 1 ;}") iSTART_OF_FINAL_STATS_REPORT" $OUTFILE
}

PREV_INFILE=""
TEST_NAMES=""
while read line; do
    RAW_FNAME=$(basename `echo "$line" | awk '{ print $1;}'`)
    TEST_NAME=`echo "$line" | awk '{ print $2; }'`
    if [ x"$RAW_FNAME" != x"$PREV_INFILE" ]; then
        run_filter
        PREV_INFILE=$RAW_FNAME
        TEST_NAMES=$TEST_NAME
    else
        TEST_NAMES="$TEST_NAMES $TEST_NAME"
    fi
done <<< "$(sort -k 1 $LIST_FILE)"

run_filter
