#! /bin/bash

LIB=$1
MALLOCS_COUNT=10000000
MAX_THREADS=32
RUNS=`seq $MAX_THREADS`
SMALL=512
VMEM_OUT=benchmark_mt_vmem.out
MALLOC_OUT=benchmark_mt.out

rm $VMEM_OUT
for i in $RUNS ; do
	./benchmark_mt -e vmem -s $SMALL $i $MALLOCS_COUNT >> $VMEM_OUT;
done

export LD_PRELOAD=$LD_PRELOAD:$LIB
rm $MALLOC_OUT
for i in $RUNS ; do
        ./benchmark_mt -e malloc -s $SMALL $i $MALLOCS_COUNT >> $MALLOC_OUT;
done

gnuplot *.p
