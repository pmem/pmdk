#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022-2023, Intel Corporation

#
# create-testconfig.sh - Script for creating testconfig files for test execution.
#

# Default location for testconfig.sh
CONF_PATH="src/test"
MOUNT_POINT=${PMDK_MOUNT_POINT:-"/mnt/pmem0"}
NON_PMEM_DIR="/dev/shm"

# Create config file for unittests.
# We are using ndctl command to gather information about devdaxes, in form known from namespace configuration.
cat >${CONF_PATH}/testconfig.sh <<EOL
# main & local
PMEM_FS_DIR=${MOUNT_POINT}
NON_PMEM_FS_DIR=${NON_PMEM_DIR}
DEVICE_DAX_PATH=($(ndctl list -X | jq -r '.[].daxregion.devices[].chardev' | awk '$0="/dev/"$0' | paste -sd' '))
KEEP_GOING=y
TEST_TIMEOUT=3m
ENABLE_SUDO_TESTS=y
EOL

# Create config file for py tests.
# We are using ndctl command to gather information about devdaxes, in form known from namespace configuration.
cat >${CONF_PATH}/testconfig.py <<EOL
config = {
    'unittest_log_level': 1,
    'page_fs_dir': '${NON_PMEM_DIR}',
    'fs': 'all',
    'cacheline_fs_dir': '${MOUNT_POINT}',
    'byte_fs_dir': '${MOUNT_POINT}',
    'force_cacheline': False,
    'force_page': False,
    'force_byte': True,
    'tm': True,
    'test_type': 'all',
    'build': 'all',
    'granularity': 'all',
    'fail_on_skip': False,
    'keep_going': True,
    'timeout': '3m',
    'fs_dir_force_pmem': 0,
    'dump_lines': 30,
    'force_enable': None,
    'device_dax_path' : [$(ndctl list -X | jq -r '.[].daxregion.devices[].chardev' | awk '$0="'\''/dev/"$0"'\''"' | paste -sd',')],
    'enable_admin_tests': True
}
EOL

