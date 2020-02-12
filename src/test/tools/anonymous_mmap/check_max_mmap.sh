#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation

#
# src/test/tools/anonymous_mmap/check_max_mmap.sh -- checks how many DAX
#               devices can be mapped under Valgrind and saves the number in
#               src/test/tools/anonymous_mmap/max_dax_devices.
#

DIR_CHECK_MAX_MMAP="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
FILE_MAX_DAX_DEVICES="$DIR_CHECK_MAX_MMAP/max_dax_devices"
ANONYMOUS_MMAP="$DIR_CHECK_MAX_MMAP/anonymous_mmap.static-nondebug"

source "$DIR_CHECK_MAX_MMAP/../../testconfig.sh"

#
# get_devdax_size -- get the size of a device dax
#
function get_devdax_size() {
	local device=$1
	local path=${DEVICE_DAX_PATH[$device]}
	local major_hex=$(stat -c "%t" $path)
	local minor_hex=$(stat -c "%T" $path)
	local major_dec=$((16#$major_hex))
	local minor_dec=$((16#$minor_hex))
	cat /sys/dev/char/$major_dec:$minor_dec/size
}

function msg_skip() {
	echo "0" > "$FILE_MAX_DAX_DEVICES"
	echo "$0: SKIP: $*"
	exit 0
}

function msg_failed() {
	echo "$0: FATAL: $*" >&2
	exit 1
}

# check if DEVICE_DAX_PATH specifies at least one DAX device
if [ ${#DEVICE_DAX_PATH[@]} -lt 1 ]; then
	msg_skip "DEVICE_DAX_PATH does not specify path to DAX device."
fi

# check if valgrind package is installed
VALGRINDEXE=`which valgrind 2>/dev/null`
ret=$?
if [ $ret -ne 0 ]; then
	msg_skip "Valgrind required."
fi

# check if memcheck tool is installed
$VALGRINDEXE --tool=memcheck --help 2>&1 | grep -qi "memcheck is Copyright (c)" && true
if [ $? -ne 0 ]; then
	msg_skip "Valgrind with memcheck required."
fi

# check if anonymous_mmap tool is built
if [ ! -f "${ANONYMOUS_MMAP}" ]; then
	msg_failed "${ANONYMOUS_MMAP} does not exist"
fi

# checks how many DAX devices can be mmapped under Valgrind and save the number
# in $FILE_MAX_DAX_DEVICES file
bytes="0"
max_devices="0"
for index in ${!DEVICE_DAX_PATH[@]}
do
	if [ ! -e "${DEVICE_DAX_PATH[$index]}" ]; then
		msg_failed "${DEVICE_DAX_PATH[$index]} does not exist"
	fi

	curr=$(get_devdax_size $index)
	if [[ curr -eq 0 ]]; then
		msg_failed "size of DAX device pointed by DEVICE_DAX_PATH[$index] equals 0."
	fi

	$VALGRINDEXE --tool=memcheck --quiet $ANONYMOUS_MMAP $((bytes + curr))
	status=$?
	if [[ status -ne 0 ]]; then
		break
	fi

	bytes=$((bytes + curr))
	max_devices=$((max_devices + 1))
done

echo "$max_devices" > "$FILE_MAX_DAX_DEVICES"

echo "$0: maximum possible anonymous mmap under Valgrind: $bytes bytes, equals to size of $max_devices DAX device(s). Value saved in $FILE_MAX_DAX_DEVICES."
