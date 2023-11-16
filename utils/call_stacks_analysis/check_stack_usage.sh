#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Check if the provided functions are present in the source *.su files.
# The main use-case for this procedure is to verify whether functions known
# to be missing from the stack_usage.txt are also not present in the source
# *.su files.
#

FUNCS_FILE=${1-zero_funcs.txt}
if [ ! -f "$FUNCS_FILE" ]; then
	echo "$FUNCS_FILE is missing"
	exit 1
fi

BUILD=${2-nondebug} # debug or nondebug

WD=$(realpath $(dirname $0))
TOP=$(realpath $WD/../..)
SRC=src/$BUILD

cd $TOP

SU_FILES="$SRC/core/*.su $SRC/common/*.su $SRC/libpmem/*.su $SRC/libpmemobj/*.su"

for func in $(cat $WD/$FUNCS_FILE); do
	func_grep=":$func\t"
	out=$(cat $SU_FILES | grep -P $func_grep)
	if [ ${#out} != 0 ]; then
		echo $func
		cat $SU_FILES | grep -P $func_grep
		echo
	fi
done

cd - > /dev/null
