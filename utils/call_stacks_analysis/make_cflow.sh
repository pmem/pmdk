#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Generate the complete calls graph for a given API functions list based on
# the cflow tool.
#

UNSAFE=$1 # '-f' to omit security checks

WD=$(realpath $(dirname "$0"))
SRC=$(realpath $WD/../../src)

API=$WD/api.txt
if [ ! -f "$API" ]; then
	echo "$API is missing"
	exit 1
fi

EXTRA_ENTRY_POINTS=$WD/extra_entry_points.txt
if [ ! -f "$EXTRA_ENTRY_POINTS" ]; then
	echo "$EXTRA_ENTRY_POINTS is missing"
	exit 1
fi

STARTS=
for start in `cat $API $EXTRA_ENTRY_POINTS`; do
	STARTS="$STARTS --start $start"
done

SOURCES=`find $SRC -name *.[ch] | grep -v -e '_other.c' -e '_none.c' \
		-e '/tools/' -e '/test/' -e '/aarch64/' -e '/examples/' \
		-e '/ppc64/' -e '/riscv64/' -e '/loongarch64/' \
		-e '/libpmempool/'`

ABORT=yes
UNCOMMITED=$(git status -s $SOURCES)

if [ "z$UNSAFE" == "z-f" ]; then
	ABORT=no
elif [ -z "$UNCOMMITED" ]; then
	ABORT=no
fi

if [ $ABORT == "yes" ]; then
	echo "The repository has uncommitted changes. Can't continue without overwriting them."
	echo "Call '$0 -f' to continue regardless."
	exit 1
fi

# Note: cflow cannot process correctly a for-loop if the initialization step of
# the loop declares a variable. It doesn't care if a variable is not defined.
# So, removing the initialization step from all the for-loops works around
# the problem.
sed -i 's/for ([^;]\+;/for (;/' $SOURCES

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
# 2.4 adjust -I parameters to use absolute paths (e.g. by using the $SRC prefix)
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

echo "Done."

# Restore the state of the files that have been modified to work around
# the cflow's for-loop problem. Please see the note above for details.
if [ ! "z$UNSAFE" == "z-f" ]; then
	git restore $SOURCES
else
	echo "Note: $0 probably modified the source code."
fi
