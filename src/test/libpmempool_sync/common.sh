#!/bin/bash -e
#
# Copyright 2016, Intel Corporation
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
#     * Neither the name of the copyright holder nor the names of its
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
#
#
# libpmempool_replica_sync/common.sh -- common functions for tests


# Corrupt data - corrupts data at specific offset from the beginning of
# root object
# corrupt_data <file_tab> <file_tab_idx> <root_off> <data> <data_len>
#
# Usage exapmple:
# declare FILES=( "file1" "file2" "file"
# corrupt_data FILES 0 8192 "Some_Data" 10
function corrupt_data {
	local -n files_arr=$1
	partno=$2
	offset_root=$3
	str=$4
	str_cnt=$5

	counter=0
	parts_data_len=0

	root_addr=$($PMEMPOOL info -f obj -o $DIR/${files_arr[$counter]} | grep "Root offset" |\
	sed 's/^Root offset[ \t]*: 0x\([0-9][0-9]*\)/\1/')
	root_addr=$((16#$root_addr))
	offset=$root_addr

	while [ $counter -lt $partno ]; do
		file_size=$(stat -c%s "$DIR/${files_arr[$counter]}")
		part_size=$(( ($file_size & $ADDR_MASK) - $offset ))
		parts_data_len=$(( $parts_data_len + $part_size ))
		let counter=counter+1
		offset=$POOL_HEADER_OFFSET
	done

	let part_off=offset_root-parts_data_len+$offset

	# Corrupt data
	echo $str | dd count=$str_cnt bs=1 seek=$part_off\
	of=$DIR/${files_arr[$partno]} conv=notrunc status=none
}
