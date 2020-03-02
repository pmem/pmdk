#!/usr/bin/env bash
#
# Copyright 2020, IBM Corporation
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
# pmempool_sync/util.sh -- utility functions for pmempool_sync test cases
#

#
# get_seek_size -- gets a rounded seek size from a base value for zero_blocks
#
# Seek sizes should be page aligned for the bad blocks sync tests. This function
# rounds out the seek value to get the closest offset for other page sizes
# reciving a value based on a 4k page size.
# Example:
# val=$(get_seek_size 1000)
# for 64k page size val will be 896 that is 458,751 bytes.
# for 4k page size val will be 1000 that is 512,000 bytes.
get_seek_size() {
        local pagesize=$(getconf PAGESIZE)
        local x=$1
        if [[ $pagesize != 4096 ]]; then
                x=$(($1*512/pagesize))
                x=$(($x*pagesize/512))
        fi
        echo $x
}

#
# get_zero_size -- gets the size of zeros to use on zero_blocks
#
# For pmempool_sync tests the zero sizes are usually multiples of the page size.
get_zero_size() {
        echo $(($(getconf PAGESIZE) / 512))
}

