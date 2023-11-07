#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Generate the complete calls graph for a given API functions list based on
# the cflow tool.
#

TOP=$(dirname "$0")/../..

API=$(dirname "$0")/api.txt
if [ ! -f "$API" ]; then
	echo "$API is missing"
	exit 1
fi

STARTS=
for start in `cat $API`; do
	STARTS="$STARTS --start $start"
done

SOURCES=`find $TOP -name *.[ch] | grep -v -e '_other.c' -e '_none.c' -e /tools/ \
		-e /test/ -e /aarch64/ -e /examples/ -e /ppc64/  -e /riscv64/ \
		-e '/loongarch64/' -e '/libpmempool/' -e '/utils/'`

echo "Code analysis may take up to 6 minutes"
echo "Start"

# --symbol list has been defined based on cflow manual
# https://www.gnu.org/software/cflow/manual/cflow.html
# Section: 6.3 GCC Initialization

# Note: the preprocess command should be an exact copy of the compile command
# as it is used in the build system. Please update if necessary.
cflow -o $(dirname "$0")/cflow.txt  \
	-i _  \
	--symbol __inline:=inline  \
	--symbol __inline__:=inline  \
	--symbol __const__:=const  \
	--symbol __const:=const  \
	--symbol __restrict:=restrict  \
	--symbol __extension__:qualifier  \
	--symbol __attribute__:wrapper  \
	--symbol __asm__:wrapper  \
	--symbol __nonnull:wrapper  \
	--symbol __wur:wrapper  \
	--symbol __thread:wrapper  \
	--symbol __leaf__:wrapper  \
	-I$TOP/src/common -I$TOP/src/core -I$TOP/src/libpmemobj  \
	-I$TOP/src/libpmem -I$TOP/src/libpmem2 -I$TOP/src/include  \
	-I$TOP/src/libpmem2/x86_64  \
	-DSDS_ENABLED -DNDCTL_ENABLED=1 -D_PMEMOBJ_INTRNL -D_FORTIFY_SOURCE=2  \
	-DSTRINGOP_TRUNCATION_SUPPORTED  \
	--preprocess='gcc -E -std=gnu99 -O2 -U_FORTIFY_SOURCE -fno-common -pthread  -fno-lto'  \
	$STARTS $SOURCES 2> $(dirname "$0")/cflow.err

echo "Done."
