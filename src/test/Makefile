#
# Copyright (c) 2014, Intel Corporation
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
#     * Neither the name of Intel Corporation nor the names of its
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
# src/test/Makefile -- build all unit tests
#
# Makefile -- build all unit tests
#
TEST = blk_nblock\
       blk_recovery\
       blk_rw\
       blk_rw_mt\
       checksum\
       log_basic\
       log_recovery\
       log_walker\
       pmem_clflush_proc\
       pmem_is_pmem\
       pmem_is_pmem_proc\
       pmem_map_raw\
       scope\
       traces\
       traces_custom_function\
       traces_pmem\
       util_map_proc\
       vmem_aligned_alloc\
       vmem_calloc\
       vmem_check_allocations\
       vmem_check_version\
       vmem_custom_alloc\
       vmem_freespace\
       vmem_malloc\
       vmem_mix_allocations\
       vmem_multiple_pools\
       vmem_out_of_memory\
       vmem_pool_check\
       vmem_pool_create\
       vmem_pool_create_error\
       vmem_pool_create_in_region\
       vmem_realloc\
       vmem_stats\
       vmem_strdup\
       vmem_valgrind\
       vmem_pages_purging

all     : TARGET = all
clean   : TARGET = clean
clobber : TARGET = clobber
test    : TARGET = test
cstyle  : TARGET = cstyle

all clean clobber test cstyle: $(TEST)

$(TEST): unittest
	$(MAKE) -C $@ $(TARGET)

unittest:
	$(MAKE) -C $@ $(TARGET)

.PHONY: all clean clobber test cstyle unittest $(TEST)
