#! /bin/bash

OPS_COUNT=100
MAX_THREADS=32
RUNS=`seq $MAX_THREADS`
LOG_IN="./log_mt.tmp"
PMEMLOG_OUT=pmemlog_mt.out
FILEIOLOG_OUT=fileiolog_mt.out

rm -f $FILEIOLOG_OUT
for i in $RUNS ; do
	./log_mt -i -v 1 -e 8192 $i $OPS_COUNT $LOG_IN >> $FILEIOLOG_OUT;
	rm -f $LOG_IN
done

rm -f $PMEMLOG_OUT
for i in $RUNS ; do
	./log_mt -v 1 -e 8192 $i $OPS_COUNT $LOG_IN >> $PMEMLOG_OUT;
	rm -f $LOG_IN
done

gnuplot gnuplot_log_mt_append.p
gnuplot gnuplot_log_mt_read.p
