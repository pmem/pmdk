#! /bin/bash

MAX_THREADS=32
OPERATIONS_PER_THREAD=200
RUNS=`seq $MAX_THREADS`
BLK_SIZE=512
BLK_FILE="./blkfile.tmp"
IO_FILE="./iofile.tmp"
FILE_SIZE=1024 #MB
PMEMBLK_OUT=benchmark_mt_pmemblk.out
FILEIOBLK_OUT=benchmark_mt_fileio.out

rm -f $IO_FILE;

./blk_mt -b $BLK_SIZE -s $FILE_SIZE -c -o $OPERATIONS_PER_THREAD $MAX_THREADS $BLK_FILE;

rm -f $PMEMBLK_OUT;

rm -f $FILEIOBLK_OUT;

for i in $RUNS ; do
	echo ./blk_mt -b $BLK_SIZE -s $FILE_SIZE -o $OPERATIONS_PER_THREAD $i $BLK_FILE;
	./blk_mt -b $BLK_SIZE -s $FILE_SIZE -o $OPERATIONS_PER_THREAD $i $BLK_FILE >> $PMEMBLK_OUT;
done

for i in $RUNS ; do
	echo ./blk_mt -b $BLK_SIZE -s $FILE_SIZE -o $OPERATIONS_PER_THREAD -i $i $IO_FILE
	./blk_mt -b $BLK_SIZE -s $FILE_SIZE -o $OPERATIONS_PER_THREAD -i $i $IO_FILE >> $FILEIOBLK_OUT;
	rm $IO_FILE;
done

gnuplot *.p
