#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation

#
# copy-to-remote-nodes.sh -- helper script used to sync remote nodes
#
set -e

if [ ! -f ../testconfig.sh ]; then
	echo "SKIP: testconfig.sh does not exist"
	exit 0
fi

# defined only to be able to source unittest.sh
UNITTEST_NAME=sync-remotes
UNITTEST_NUM=0

# Override default FS (any).
# This is not a real test, so it should not depend on whether
# PMEM_FS_DIR/NON_PMEM_FS_DIR are set.
FS=none

. ../unittest/unittest.sh

COPY_TYPE=$1
shift

case "$COPY_TYPE" in
	common)
		copy_common_to_remote_nodes $* > /dev/null
		exit 0
                ;;
	test)
		copy_test_to_remote_nodes $* > /dev/null
		exit 0
                ;;
esac

echo "Error: unknown copy type: $COPY_TYPE"
exit 1
