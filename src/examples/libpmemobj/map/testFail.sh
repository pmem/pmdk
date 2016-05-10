#!/bin/bash

PMEMBENCH_DIR=/media/ramdisk
#PMEMBENCH_DIR=.
GROUPSIZE=100
NAME=Fail${GROUPSIZE}group
for i in `seq 1 10`;
do
    for t in ctree rbtree btree;
    do
        file=$PMEMBENCH_DIR/$t
        if [ -f $file ] ; then
            rm $file
        fi

        OUTPUT=${NAME}_Fail$((GROUPSIZE/2))_${t}_${i}.csv
        echo $file
        echo $OUTPUT
        echo $((GROUPSIZE/2))
        ./mapcli $t $file 1461895662 10 10000 $((GROUPSIZE/2)) | tee $OUTPUT
        rm $file

    done
done