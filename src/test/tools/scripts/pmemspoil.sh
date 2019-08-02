#!/usr/bin/env bash
#
# Copyright 2019, Intel Corporation
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
# src/test/tools/scripts/pmemspoil.sh -- tool script for pmemspoil tool
#

pool=$1
shift

for op in $*; do
	case "$op" in
		hdr_inv_signature_and_checksum_gen)
			../pmemspoil $pool pool_hdr.signature=ERROR \
				"pool_hdr.checksum_gen()"
			;;
		hdr_inv_checksum)
			../pmemspoil $pool pool_hdr.checksum=0
			;;
		sds_set_dirty)
			../pmemspoil $pool pool_hdr.shutdown_state.usc=999 \
				pool_hdr.shutdown_state.dirty=1 \
				"pool_hdr.shutdown_state.checksum_gen()"
			;;
		hdr_enable_sds_and_checksum_gen)
			../pmemspoil $pool pool_hdr.features.incompat=0x0006 \
				"pool_hdr.checksum_gen()"
			;;
	esac
done
