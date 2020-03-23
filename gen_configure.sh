#!/bin/bash

BASEDIR="$(dirname "$0")"
cd "$BAESDIR"

if [ $# -ge 1 ]; then
    rm -rf autom4te.cache config.h.in stamp-h.in configure
else
    autoconf
    autoheader
    echo timestamp > stamp-h.in
    rm -f config.h.in~
fi
