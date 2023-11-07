#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Combine stack usage into a file. The script shall be run from the main PMDK folder.
#

for build in debug nondebug; do
	grep -v ^$ src/$build/core/*.su src/$build/common/*.su \
			src/$build/libpmem/*.su src/$build/libpmemobj/*.su | \
		gawk -F "[:\t]" '{print $6 " " $5 " : " $1 ":" $2 " " $7}' | \
		sort -n -r > $(dirname "$0")/stack_usage_$build.txt
done
