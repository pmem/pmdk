#!/usr/bin/env bash
#
# Copyright 2016-2018, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

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
