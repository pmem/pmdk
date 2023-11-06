#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Generate, based on cflow tool the complete calls graph for
# a given api functions list.
# The script shall be run from the main PMDK folder.
#
# usage: cflow.sh api.txt
#

if [ -z "$1" ]; then
	echo "Usage: cflow.sh api.txt"
	exit 1
fi

STARTS=
for start in `cat $1`; do
	STARTS="$STARTS --start $start"
done

SOURCES=
for source in `find . -name *.[ch] | grep -v -e '_other.c' -e '_none.c' -e /tools/ -e /test/ -e /aarch64/ -e /examples/ -e /ppc64/  -e /riscv64/ -e '/loongarch64/' -e '/libpmempool/' -e'/libpmem2/'`; do
	SOURCES="$SOURCES $source"
done

cflow -o $(dirname "$0")/cflow.txt  \
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
	--preprocess='gcc -E -I. -I.. -I./src/common -I./src/core -I./src/libpmemobj -I./src/libpmem -I./src/libpmem2 -I./src/include -I./src/libpmem2/x86_64 -std=gnu99 -Wall -Wmissing-prototypes -Wpointer-arith -Wsign-conversion -Wsign-compare -Wunused-parameter -fstack-usage -Wconversion -Wmissing-field-initializers -Wfloat-equal -Wswitch-default -Wcast-function-type -DSTRINGOP_TRUNCATION_SUPPORTED -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -std=gnu99 -fno-common -pthread -DSRCVERSION=\"2.0.0+git53.g7f8cd6114\" -fno-lto -DSDS_ENABLED -DNDCTL_ENABLED=1 -fstack-usage'  \
	$STARTS $IGNORE_STR $SOURCES 2> $(dirname "$0")/cflow.err
