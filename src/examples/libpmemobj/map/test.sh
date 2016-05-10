#!/bin/bash

PMEMBENCH_DIR=/media/ramdisk
#PMEMBENCH_DIR=.
NAME=10group
for i in `seq 1 10`;
do
    for t in ctree rbtree btree;
    do
        file=$PMEMBENCH_DIR/$t
        if [ -f $file ] ; then
            rm $file
        fi

        OUTPUT=${NAME}_${t}_${i}.csv
        echo $file
        echo $OUTPUT
        ./mapcli $t $file 1461895662 10 10000 | tee $OUTPUT
        rm $file

    done
done