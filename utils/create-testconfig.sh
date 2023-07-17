#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# create-testconfig.sh - creates testconfig files for test execution
#
# Note: It dedicates all Device DAX devices found in the system for testing
# purposes.
#

# This script makes use of the following variables as they are available in
# the environment:
# - OUTPUT_DIR - the directory where the output testconfig.[sh|py] files are
#   about to be written to
# - PMEM_FS_DIR - a directory of a PMem-aware file system
# - PMEM_FS_DIR_FORCE_PMEM - set to 1, if MAP_SYNC in the PMEM_FS_DIR filesystem
#   will cause an error. Set to 0 otherwise.
# - force_cacheline - set to True to act in accordance with cache line
#   granularity. Set to False otherwise.
# - force_byte - set to True to act in accordance with byte granularity. Set to
#   False otherwise.

# loading defaults if necessary
OUTPUT_DIR=${OUTPUT_DIR:-src/test}
PMEM_FS_DIR=${PMEM_FS_DIR:-/mnt/pmem0}
PMEM_FS_DIR_FORCE_PMEM=${PMEM_FS_DIR_FORCE_PMEM:-0}
force_cacheline=${force_cacheline:-False}
force_byte=${force_byte:-True}

# converts y/n value to True/False value
function ynToTrueFalse() {
    [ "$1" = "y" ] && echo "True" || echo "False"
}

# static values for both Bash and Python
NON_PMEM_FS_DIR=/dev/shm
KEEP_GOING=y
keep_going=$(ynToTrueFalse $KEEP_GOING)
TEST_TYPE=check
TEST_BUILD="debug nondebug"
build="['debug', 'nondebug']"
TEST_TIMEOUT=120m
ENABLE_SUDO_TESTS=y
enable_admin_tests=$(ynToTrueFalse $ENABLE_SUDO_TESTS)
TM=1
[ "$TM" = "1" ] && tm=True || tm=False

# lookup all Device DAX devices in the system
DEVICE_DAX_PATH=
device_dax_path=
if [ `which ndctl` -a `which jq` ]; then
    DEVICE_DAX_PATH="$(ndctl list -X | jq -r '.[].daxregion.devices[].chardev' | awk '$0="/dev/"$0' | paste -sd' ')"
    device_dax_path="$(ndctl list -X | jq -r '.[].daxregion.devices[].chardev' | awk '$0="'\''/dev/"$0"'\''"' | paste -sd',')"
fi

# generate Bash's test configuration file
cat << EOF > $OUTPUT_DIR/testconfig.sh
NON_PMEM_FS_DIR=$NON_PMEM_FS_DIR
PMEM_FS_DIR=$PMEM_FS_DIR
PMEM_FS_DIR_FORCE_PMEM=$PMEM_FS_DIR_FORCE_PMEM
DEVICE_DAX_PATH=($DEVICE_DAX_PATH)
KEEP_GOING=$KEEP_GOING
TEST_TYPE=$TEST_TYPE
TEST_BUILD="$TEST_BUILD"
TEST_TIMEOUT=$TEST_TIMEOUT
ENABLE_SUDO_TESTS=$ENABLE_SUDO_TESTS
TM=$TM
EOF

# generate Python's test configuration file
cat << EOF > $OUTPUT_DIR/testconfig.py
config = {
    'page_fs_dir': '${NON_PMEM_FS_DIR}',
    'force_page': False,
    'cacheline_fs_dir': '${PMEM_FS_DIR}',
    'force_cacheline': $force_cacheline,
    'byte_fs_dir': '${PMEM_FS_DIR}',
    'force_byte': $force_byte,
    'device_dax_path' : [$device_dax_path],
    'keep_going': $keep_going,
	'test_type': '$TEST_TYPE',
    'build': $build,
	'granularity': 'all',
    'timeout': '$TEST_TIMEOUT',
    'enable_admin_tests': $enable_admin_tests,
    'tm': $tm,
}
EOF
