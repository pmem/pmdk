#! /bin/bash
#
# Copyright (c) 2014-2015, Intel Corporation
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

OPS_COUNT=100
MAX_THREADS=32
RUNS=`seq $MAX_THREADS`
DIR="."
PMEMLOG_OUT=pmemlog_mt.out
FILEIOLOG_OUT=fileiolog_mt.out
[ -n "$1" ] && DIR=$1
[ -n "$2" ] && OPS_COUNT=$2
LOG_IN="${DIR}/log_mt.tmp"

rm -f $FILEIOLOG_OUT
rm -f $LOG_IN

for i in $RUNS ; do
	echo ./log_mt -i -v 1 -e 8192 $i $OPS_COUNT $LOG_IN;
	./log_mt -i -v 1 -e 8192 $i $OPS_COUNT $LOG_IN >> $FILEIOLOG_OUT;
	rm -f $LOG_IN
done

rm -f $PMEMLOG_OUT
for i in $RUNS ; do
	echo ./log_mt -v 1 -e 8192 $i $OPS_COUNT $LOG_IN;
	./log_mt -v 1 -e 8192 $i $OPS_COUNT $LOG_IN >> $PMEMLOG_OUT;
	rm -f $LOG_IN
done

gnuplot gnuplot_log_mt_append.p
gnuplot gnuplot_log_mt_read.p
