#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Combine stack usage into a file.
#

TOP=$(realpath $(dirname $0)/../..)

for build in debug nondebug; do
	SU_FILES="$TOP/src/$build/core/*.su $TOP/src/$build/common/*.su \
		$TOP/src/$build/libpmem/*.su $TOP/src/$build/libpmemobj/*.su"

	grep -v ^$ $SU_FILES | \
		gawk -F "[:\t]" '{print $6 " " $5 " : " $1 ":" $2 " " $7}' | \
		sort -n -r > $(dirname "$0")/stack_usage_$build.txt
done
