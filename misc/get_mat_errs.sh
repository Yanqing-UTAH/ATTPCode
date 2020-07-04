#!/bin/bash

if [ $# -lt 1 ]; then
    echo "usage: $0 <infile>"
    exit 1
fi

grep 'ATA' $1 | awk '{ print $1, $6 }' | sort -k 1 |
awk '
function print_err(cur) {
    if (prev != "") {
        print prev, min_err, max_err, sum_err / n;
    }
    prev = cur
    min_err = 100
    max_err = 0
    sum_err = 0.0
    n = 0
}
BEGIN{
    prev = "";
    print "test: min_err max_err avg_err";
    print_err("");
}
{
    if ($1 != prev) {
        print_err($1);
    }
    if ($2 < min_err) {
        min_err = $2; 
    }
    if ($2 > max_err) {
        max_err = $2;
    }
    sum_err += $2;
    n += 1;
}
END {
    print_err("");
}
'
