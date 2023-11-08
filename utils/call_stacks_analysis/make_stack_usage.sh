#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Combine stack usage into a file.
#

BUILD=${1-nondebug} # debug or nondebug

WD=$(realpath $(dirname $0))
TOP=$WD../..

SU_FILES="$TOP/src/$BUILD/core/*.su $TOP/src/$BUILD/common/*.su \
	$TOP/src/$BUILD/libpmem/*.su $TOP/src/$BUILD/libpmemobj/*.su"

grep -v ^$ $SU_FILES | \
	gawk -F "[:\t]" '{print $6 " " $5 " : " $1 ":" $2 " " $7}' | \
	sort -n -r > $WD/stack_usage.txt
