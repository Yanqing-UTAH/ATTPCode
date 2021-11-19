#!/bin/bash

BASEDIR="$(realpath "`dirname "$0"`")"
cd "$BASEDIR"

for i in client_id_attp client_id_bitp object_id_attp object_id_bitp_new \
	ms_small_attp ms_medium_attp ms_big_attp; do
	./filter.sh ${i}_list.txt ${i}_filtered
	cat ${i}_filtered/* > ${i}_filtered_combined.txt
done

