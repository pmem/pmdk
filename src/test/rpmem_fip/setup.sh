#!/usr/bin/env bash
#
# Copyright 2016-2017, Intel Corporation
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
# src/test/rpmem_fip/setup.sh -- common setup for rpmem_fip tests
#
set -e

require_nodes 2
require_node_libfabric 0 $RPMEM_PROVIDER
require_node_libfabric 1 $RPMEM_PROVIDER
require_node_log_files 0 $RPMEM_LOG_FILE $RPMEMD_LOG_FILE
require_node_log_files 1 $RPMEM_LOG_FILE $RPMEMD_LOG_FILE

SRV=srv${UNITTEST_NUM}.pid
clean_remote_node 0 $SRV
RPMEM_CMD="\"cd ${NODE_TEST_DIR[0]} && RPMEMD_LOG_LEVEL=\$RPMEMD_LOG_LEVEL RPMEMD_LOG_FILE=\$RPMEMD_LOG_FILE UNITTEST_FORCE_QUIET=1 \
	LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$REMOTE_LD_LIBRARY_PATH:${NODE_LD_LIBRARY_PATH[0]} \
	./rpmem_fip$EXESUFFIX\""

export_vars_node 1 RPMEM_CMD

if [ -n ${RPMEM_MAX_NLANES+x} ]; then
	export_vars_node 1 RPMEM_MAX_NLANES
fi
