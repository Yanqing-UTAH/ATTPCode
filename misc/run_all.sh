#!/bin/bash

while read -r line; do
    PARAM=`echo "$line" | awk '{ print $1; }'`
    OUTFILE=`echo "$line" | awk '{ print $2; }'`
    SERVER=`echo "$line" | awk '{ print $3; }'`
    
    cat test.conf | sed "s/<param1>/$PARAM/" |
    ssh $SERVER '
        cd $MYHOME/ATTPCode;
        cat > test.conf;
        grep '"'"'PCM_HH.epsilon'"'"' test.conf
        nohup bash -c -v '"'"'
            echo "Running '$PARAM' on $MYNAME to '$OUTFILE'";
            ./driver run test.conf > '$OUTFILE'
        '"'"' < /dev/null &> /dev/null &
    '
done < run_list.txt
