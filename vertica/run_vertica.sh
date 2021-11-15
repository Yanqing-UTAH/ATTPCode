#!/bin/bash

BASEDIR="$(realpath "`dirname "$0"`")"
cd "$BASEDIR"

DATADIR="$(realpath "$BASEDIR/../data/cid_10x_vertica")"

make
./scalability_test_client_id attp "$DATADIR" f | tee ../output/scalability-test-vertica-wo-preagg.log
./scalability_test_client_id attp "$DATADIR" t | tee ../output/scalability-test-vertica-w-preagg.log
