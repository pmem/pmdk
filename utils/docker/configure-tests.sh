#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# configure-tests.sh - is called inside a Docker container; configures tests
#						for use during build of PMDK project.
#

set -e

# Configure tests
cat << EOF > $WORKDIR/src/test/testconfig.sh
LONGDIR=LoremipsumdolorsitametconsecteturadipiscingelitVivamuslacinianibhattortordictumsollicitudinNullamvariusvestibulumligulaetegestaselitsemperidMaurisultriciesligulaeuipsumtinciduntluctusMorbimaximusvariusdolorid
# this path is ~3000 characters long
DIRSUFFIX="$LONGDIR/$LONGDIR/$LONGDIR/$LONGDIR/$LONGDIR"
NON_PMEM_FS_DIR=/tmp
PMEM_FS_DIR=/tmp
PMEM_FS_DIR_FORCE_PMEM=1
TEST_BUILD="debug nondebug"
ENABLE_SUDO_TESTS=y
TM=1
EOF

# Configure python tests
	cat << EOF >> $WORKDIR/src/test/testconfig.py
config = {
	'unittest_log_level': 1,
	'cacheline_fs_dir': '/tmp',
	'force_cacheline': True,
	'page_fs_dir': '/tmp',
	'force_page': False,
	'byte_fs_dir': '/tmp',
	'force_byte': True,
	'tm': True,
	'test_type': 'check',
	'granularity': 'all',
	'fs_dir_force_pmem': 0,
	'keep_going': False,
	'timeout': '3m',
	'build': ['debug', 'release'],
	'force_enable': None,
	'device_dax_path': [],
	'fail_on_skip': False,
	'enable_admin_tests': True
   }
EOF
