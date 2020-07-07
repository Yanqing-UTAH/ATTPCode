#!/bin/bash

# assume we are running from repo root
BASEDIR="."

if [ ! -d deps ]; then
    mkdir deps
    if [ ! -d deps/src ]; then
        mkdir deps/src
    fi
else
    rm -f deps/src/*
fi

FILE_LIST="`find . -maxdepth 1 -a \( -name '*.cpp' -or -name '*.c' \) `"

for src_file in $FILE_LIST; do
    dirname=`dirname "$src_file"`
    basename=`basename "$src_file" | sed 's/[.]\(cpp\|c\)//'`
    dep_filebase="$BASEDIR/deps/src/${dirname}/${basename}"
    suffix=`basename "$src_file" | sed 's/^.*[.]\(cpp\|c\)$/\1/'`
    if [ x'$suffix' = xc ]; then
        export COMPILER="$CC"
    else
        export COMPILER="$CXX"
    fi
    $@ -MMD -MF "${dep_filebase}.d.raw" -E "$BASEDIR/${src_file}" -o /dev/null || exit 1
    sed "s,[$BASEDIR]/,,g" "${dep_filebase}.d.raw" |
    sed 's,/[a-z0-9./-]*[.]h,,g' > "${dep_filebase}.d"
    echo -e '' >> "${dep_filebase}.d"
done

if [ -f "$BASEDIR/deps/all_deps.d" ]; then
    mv "$BASEDIR/deps/all_deps.d" "$BASEDIR/deps/all_deps.d.bak"
fi

echo "" > "$BASEDIR/deps/all_deps.d"
cat `find "$BASEDIR/deps" -mindepth 2 -name '*.d'` >> "$BASEDIR/deps/all_deps.d"
echo "" >> "$BASEDIR/deps/all_deps.d"


OBJS=$(grep '^.*[.]o:' "$BASEDIR/deps/all_deps.d" | sed 's,:.*,,' | tr '\n' ' ')
DRIVER_OBJS=$(grep '^.*[.]o:' "$BASEDIR/deps/all_deps.d" | \
    grep -v '^test_' | sed 's,:.*,,' | tr '\n' ' ')

mv Makefile.in Makefile.in.old

sed '
    /# objs/,/# end of objs/{
        /# end of objs/{
            r deps/all_deps.d
            N
        }
        /^#/!d
    }' Makefile.in.old |\
sed "s/^OBJS=."'*'"/OBJS=${OBJS}/" |\
sed "s/^DRIVER_OBJS=."'*'"/DRIVER_OBJS=${DRIVER_OBJS}/" > Makefile.in

rm -f Makefile.in.old

