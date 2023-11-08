#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Combine stack usage into a file.
#

BUILD=${1-nondebug} # debug or nondebug

WD=$(realpath $(dirname $0))
SRC=$(realpath $WD/../../src)

SU_FILES="$SRC/$BUILD/core/*.su $SRC/$BUILD/common/*.su \
	$SRC/$BUILD/libpmem/*.su $SRC/$BUILD/libpmemobj/*.su"

grep -v ^$ $SU_FILES | \
	gawk -F "[:\t]" '{print $6 " " $5 " : " $1 ":" $2 " " $7}' | \
	sort -n -r > $WD/stack_usage.txt
