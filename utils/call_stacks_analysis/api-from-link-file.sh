#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Generate a list of all libpmem and libpmemobj public API functions.
# The script shall be run from the main PMDK folder.
#

grep ";" src/libpmem/libpmem.link.in src/libpmemobj/libpmemobj.link.in | \
	grep -v -e'*' -e'}' -e'_pobj_cache' | \
	gawk -F "[;\t]" '{ print $3 }' | sort |  uniq  > $(dirname "$0")/api.txt
