#!/bin/bash

export PMEMBENCH_DIR=/media/ramdisk
for i in $(seq 1 2500);
do
	echo bench_map$i.log
	./pmembench pmembench_map.cfg | tee bench_map$i.log
done
