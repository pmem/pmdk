#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Generate a list of all libpmem and libpmemobj public API functions.
#

WD=$(realpath $(dirname $0))
SRC=$(realpath $WD/../../src)

LINK_FILES="$SRC/libpmem/libpmem.link.in $SRC/libpmemobj/libpmemobj.link.in"

for link in $LINK_FILES; do
	if [ ! -f $link ]; then
		echo "$link is missing"
		exit 1
	fi
done

API=$WD/api.txt

grep ";" $LINK_FILES | \
	grep -v -e'*' -e'}' -e'_pobj_cache' | \
	gawk -F "[;\t]" '{ print $3 }' | sort |  uniq  > $API

for api in libpmem_init libpmem_fini libpmemobj_init libpmemobj_fini; do
	echo $api >> $API
done
