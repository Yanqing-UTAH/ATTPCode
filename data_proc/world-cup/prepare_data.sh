#!/bin/bash

## remove ../../data/raw_world_cup_data if you want to create the dataset
## from scratch

BASEDIR="$(realpath "`dirname "$0"`")"
cd "$BASEDIR"

DATADIR=../../data
mkdir -p $DATADIR

if [ ! -d $DATADIR ]; then
    echo "failed to create directory $DATADIR"
    exit 1
fi

make || exit 1

if [ ! -x ../insert_mem_stats ]; then
    g++ -std=gnu++17 -O2 -o ../insert_mem_stats ../insert_mem_stats.cpp
fi

if [ ! -f $DATADIR/WorldCup.html ]; then
    curl -o $DATADIR/WorldCup.html ftp://ita.ee.lbl.gov/html/contrib/WorldCup.html || exit 1
fi

grep '<a href="ftp.*wc_day[0-9_]*[.]gz' $DATADIR/WorldCup.html |
    sed 's/<a href="//' |
    sed 's/">.*//' > $DATADIR/wc_url_list.txt

mkdir -p $DATADIR/raw_world_cup_data
if [ ! -d $DATADIR/raw_world_cup_data ]; then
    echo "failed to create directory $DATADIR/raw_world_cup_data"
    exit 1
fi

echo "Downloading and decompressing data"
rm -f $DATADIR/wc_raw_file_list.txt
for url in `cat $DATADIR/wc_url_list.txt`; do
    gzfile=$(basename $url)
    rawfile=$(echo $gzfile | sed 's/[.]gz//')
    echo $rawfile >> $DATADIR/wc_raw_file_list.txt
    if [ ! -f $DATADIR/raw_world_cup_data/$rawfile ]; then
        if [ ! -f $DATADIR/raw_world_cup_data/$gzfile ]; then
            curl -o $DATADIR/raw_world_cup_data/$gzfile $url
        fi
        if [ -f $DATADIR/raw_world_cup_data/$gzfile ]; then
            REMOTE_SIZE=$(curl -I "$url" 2>/dev/null | tr '\r' '\n' | grep "Content-Length" | awk '{ print $2; }')
            LOCAL_SIZE=$(ls -l $DATADIR/raw_world_cup_data/$gzfile | awk '{ print $5; }')
            if [ $REMOTE_SIZE -ne $LOCAL_SIZE ]; then
                echo curl -o $DATADIR/raw_world_cup_data/$gzfile $url
            fi
        fi
        gzip -d $DATADIR/raw_world_cup_data/$gzfile
    fi
done

INFILES="$(cat $DATADIR/wc_raw_file_list.txt | sed "s,^,$DATADIR/raw_world_cup_data/," | tr '\n' ' ')"

echo "Creating world-cup-98-client-id.txt"
./create_client_id_log $DATADIR/world-cup-98-client-id.txt $INFILES
NLINES=`cat $DATADIR/world-cup-98-client-id.txt | wc -l`
../insert_mem_stats $DATADIR/world-cup-98-client-id.txt $DATADIR/world-cup-98-client-id-w-stats-report.txt $NLINES 100
cat $DATADIR/world-cup-98-client-id-w-stats-report.txt client_id_attp_queries.txt > $DATADIR/wc-client-id-attp-w-stats-report.txt
cat $DATADIR/world-cup-98-client-id-w-stats-report.txt client_id_bitp_queries.txt > $DATADIR/wc-client-id-bitp-w-stats-report.txt

echo "Creating world-cup-98-object-id.txt"
./create_object_id_log $DATADIR/world-cup-98-object-id.txt $INFILES
../insert_mem_stats $DATADIR/world-cup-98-object-id.txt $DATADIR/world-cup-98-object-id-w-stats-report.txt $NLINES 100
cat $DATADIR/world-cup-98-object-id-w-stats-report.txt object_id_attp_queries.txt > $DATADIR/wc-object-id-attp-w-stats-report.txt
cat $DATADIR/world-cup-98-object-id-w-stats-report.txt object_id_bitp_queries.txt > $DATADIR/wc-object-id-bitp-w-stats-report.txt

#echo "Creating world-cup-98-client-ip.txt"
#./create_client_ip_log $DATADIR/world-cup-98-client-ip.txt $INFILES
#../insert_mem_stats $DATADIR/world-cup-98-client-ip.txt $DATADIR/world-cup-98-client-ip-w-stats-report.txt $NLINES 100
#cat $DATADIR/world-cup-98-client-ip-w-stats-report.txt client_id_attp_queries.txt > $DATADIR/wc-client-ip-attp-w-stats-report.txt
#cat $DATADIR/world-cup-98-client-ip-w-stats-report.txt client_id_bitp_queries.txt > $DATADIR/wc-client-ip-bitp-w-stats-report.txt

