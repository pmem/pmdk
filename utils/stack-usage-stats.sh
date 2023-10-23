# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
# utils/stack-usage-stat.sh -- combine stack usage into a file
# The script shall be run from the main PMDK folder.
#

if [ ! -d "src/stats" ]; then
	mkdir src/stats
fi

for build in debug nondebug; do
	grep -v ^$ src/$build/*/*.su | \
	gawk -F "[:\t]" '{print $6 " " $5 " : " $1 ":" $2 " " $7}' | \
	sort -n -r > src/stats/stack-usage-$build.txt
done
