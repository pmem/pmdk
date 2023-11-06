#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Generate the complete calls graph for a given API functions list based on
# the cflow tool.
#

WD=$(realpath $(dirname "$0"))
SRC=$(realpath $WD/../../scr)

API=$WD/api.txt

if [ ! -f "$API" ]; then
	echo "$API is missing"
	exit 1
fi

STARTS=
for start in `cat $API`; do
	STARTS="$STARTS --start $start"
done

cd $SRC

SOURCES=`find . -name *.[ch] | grep -v -e '_other.c' -e '_none.c' \
		-e /tools/ -e /test/ -e /aarch64/ -e /examples/ -e /ppc64/ \
		-e /riscv64/ -e '/loongarch64/' -e '/libpmempool/'`

# Note: --symbol list has been defined based on cflow manual
# https://www.gnu.org/software/cflow/manual/cflow.html
# Section: 6.3 GCC Initialization

# Note: The preprocess argument and used defines and includes should mirror
# the compile command as it is used in the build system. Please update if
# necessary.
#
# To build a new preprocess command:
# 1. run `make libpmemobj` command in the PMDK/src folder
# 2. take one of the `cc ...` lines and:
# 2.1 remove all -W, -o, -MD, and -c parameters and any file name that is included in the command line
# 2.2 take -D parameters and add them directly to the cflow command
# 2.3 take -I parameters and add them directly to the cflow command
# 2.4 adjust -I parameters to use absolute paths (e.g. by using $SRC prefix)
# 2.5 update the --preprocess argument with the remaining part of your initial compile command.
# Repeat the above steps with 'libpmem' instead of 'libpmemobj'.
# You do not need to add anything twice.

echo "Code analysis may take more than 5 minutes. Go get yourself a coffee."

echo "Start"

cflow -o $WD/cflow.txt \
	-i _ \
	--symbol __inline:=inline \
	--symbol __inline__:=inline \
	--symbol __const__:=const \
	--symbol __const:=const \
	--symbol __restrict:=restrict \
	--symbol __extension__:qualifier \
	--symbol __attribute__:wrapper \
	--symbol __asm__:wrapper \
	--symbol __nonnull:wrapper \
	--symbol __wur:wrapper \
	--symbol __thread:wrapper \
	--symbol __leaf__:wrapper \
	-I$SRC/include -I$SRC/common/ -I$SRC/core/ \
	-I$SRC/libpmem2/x86_64 -I$SRC/libpmem2 \
	-DSTRINGOP_TRUNCATION_SUPPORTED -D_FORTIFY_SOURCE=2 \
	-DSRCVERSION=\"2.0.0+git50.g22473a25d\" \
	-DSDS_ENABLED -DNDCTL_ENABLED=1 -D_PMEMOBJ_INTRNL \
	-DAVX512F_AVAILABLE=1 -DMOVDIR64B_AVAILABLE=1 \
	--preprocess='gcc -E -std=gnu99 -fstack-usage -O2 -U_FORTIFY_SOURCE -fno-common -pthread -fno-lto -fPIC' \
	$STARTS $SOURCES 2> $WD/cflow.err

cd - > /dev/null

echo "Done."
