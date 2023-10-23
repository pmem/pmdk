#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
# utils/stack-usage-stat.sh -- combine stack usage into a file
# The script shall be run from the main PMDK folder.
#

if [ ! -d "src/stat" ] ; then
	mkdir src/stat
fi

fgrep -r -e static -e dynamic src | grep '.su:' | \
    grep -e nondebug | \
    gawk '{print $2 " "  $1 " " $3 " "  $4 " "  $5}' | \
    sort -n -r > src/stat/stack-usage-nondebug.txt

sed -i 's/:[0-9]*:[0-9]//g' src/stat/stack-usage-nondebug.txt

fgrep -r -e static -e dynamic src | grep '.su:' | \
    grep -e '\/debug\/' | \
    gawk '{print $2 " "  $1 " " $3 " "  $4 " "  $5}' | \
    sort -n -r > src/stat/stack-usage-debug.txt

sed -i 's/:[0-9]*:[0-9]//g' src/stat/stack-usage-debug.txt
