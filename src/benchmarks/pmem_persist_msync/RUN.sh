#! /bin/bash
#
# Copyright (c) 2015, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

POOL_FILE="./testfile1.tmp"
ITERATIONS=100000
NUM_TESTS=21
SIZE_START=1
SIZE_MULTIPLIER=2

ADJUST_SIZE=1
MAX_SIZE=100000000000 # 100 GB

[ -n "$1" ] && POOL_FILE=$1
[ -n "$2" ] && ITERATIONS=$2
[ -n "$3" ] && NUM_TESTS=$3
[ -n "$4" ] && SIZE_START=$4
[ -n "$5" ] && SIZE_MULTIPLIER=$5

RUNS=`seq $NUM_TESTS`
LOG_OUT=pmem_persist_msync.out

rm -f $LOG_OUT

DATA_SIZE=$SIZE_START

for i in $RUNS ; do
	rm -f $POOL_FILE
	if [ $ADJUST_SIZE -eq 1 -a $(($DATA_SIZE * $ITERATIONS)) -gt $MAX_SIZE ]; then
		ITERATIONS=$(($ITERATIONS / $SIZE_MULTIPLIER))
		echo "$0: ITERATIONS decreased to $ITERATIONS"
	fi

	echo "[#$i/$NUM_TESTS] ./pmem_persist_msync $POOL_FILE $DATA_SIZE $ITERATIONS"
	./pmem_persist_msync $POOL_FILE $DATA_SIZE $ITERATIONS >> $LOG_OUT

	DATA_SIZE=$(($DATA_SIZE * $SIZE_MULTIPLIER));
done

gnuplot gnuplot_pmem_persist_and_msync.p
gnuplot gnuplot_pmem_persist_divby_msync.p
