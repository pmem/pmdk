#!/bin/bash -e
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
# src/test/rpmem_basic/setup.sh -- common part for TEST* scripts
#

require_nodes 2
require_node_libfabric 0 $RPMEM_PROVIDER $SETUP_LIBFABRIC_VERSION
require_node_libfabric 1 $RPMEM_PROVIDER $SETUP_LIBFABRIC_VERSION
require_node_log_files 0 $RPMEMD_LOG_FILE
require_node_log_files 1 $RPMEM_LOG_FILE
require_node_log_files 1 $PMEM_LOG_FILE

POOLS_DIR=pools
POOLS_PART=pool_parts
PART_DIR=${NODE_TEST_DIR[0]}/$POOLS_PART
RPMEM_POOLSET_DIR=${NODE_TEST_DIR[0]}/$POOLS_DIR

if [ -z "$SETUP_MANUAL_INIT_RPMEM" ]; then
	init_rpmem_on_node 1 0
fi
