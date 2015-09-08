#
# Copyright (c) 2014-2015, Intel Corporation
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

TEST_DEPS = \
       unittest\
       pmemspoil\
       pmemwrite\
       pmemalloc\
       obj_list

TEST = blk_nblock\
       blk_non_zero\
       blk_pool\
       blk_pool_lock\
       blk_recovery\
       blk_rw\
       blk_rw_mt\
       checksum\
       log_basic\
       log_pool\
       log_pool_lock\
       log_recovery\
       log_walker\
       pmem_isa_proc\
       pmem_is_pmem\
       pmem_is_pmem_proc\
       pmem_map\
       pmem_memmove\
       pmem_memcpy\
       pmem_movnt_align\
       pmem_memset\
       pmem_valgr_simple\
       pmem_movnt\
       scope\
       traces\
       traces_custom_function\
       traces_pmem\
       util_map_proc\
       util_file_create\
       util_file_open\
       util_poolset\
       util_poolset_parse\
       set_funcs\
       vmem_aligned_alloc\
       vmem_calloc\
       vmem_check_allocations\
       vmem_check_version\
       vmem_custom_alloc\
       vmem_malloc\
       vmem_malloc_usable_size\
       vmem_mix_allocations\
       vmem_multiple_pools\
       vmem_out_of_memory\
       vmem_check\
       vmem_create\
       vmem_create_error\
       vmem_create_in_region\
       vmem_delete\
       vmem_realloc\
       vmem_realloc_inplace\
       vmem_stats\
       vmem_strdup\
       vmem_valgrind\
       vmem_pages_purging\
       vmmalloc_calloc\
       vmmalloc_check_allocations\
       vmmalloc_fork\
       vmmalloc_init\
       vmmalloc_malloc\
       vmmalloc_malloc_hooks\
       vmmalloc_malloc_usable_size\
       vmmalloc_memalign\
       vmmalloc_out_of_memory\
       vmmalloc_realloc\
       vmmalloc_valgrind\
       vmmalloc_valloc\
       pmempool_check\
       pmempool_info\
       pmempool_dump\
       pmempool_create\
       pmempool_help\
       magic\
       arch_flags\
       obj_redo_log\
       obj_basic_integration\
       obj_pmalloc_basic\
       obj_pool\
       obj_pool_lock\
       obj_lane\
       obj_sync\
       obj_direct\
       obj_cuckoo\
       obj_list_insert\
       obj_list_insert_new\
       obj_list_remove\
       obj_list_remove_free\
       obj_list_move\
       obj_list_move_oob\
       obj_list_realloc\
       obj_list_realloc_move\
       obj_list_valgrind\
       obj_list_recovery\
       obj_tx_flow\
       obj_tx_alloc\
       obj_tx_free\
       obj_tx_add_range\
       obj_tx_add_range_direct\
       obj_tx_strdup\
       obj_tx_realloc\
       obj_tx_locks\
       obj_tx_locks_abort\
       obj_ctree\
       obj_bucket\
       obj_heap\
       obj_store\
       obj_debug\
       obj_recovery\
       obj_recreate\
       out_err\
       out_err_mt\
       obj_pmalloc_mt\
       obj_many_size_allocs\
       obj_heap_state\
       obj_check\
       obj_out_of_memory\
       obj_pmalloc_oom_mt

all     : TARGET = all
clean   : TARGET = clean
clobber : TARGET = clobber
test    : TARGET = test
cstyle  : TARGET = cstyle
check   : TARGET = check
pcheck  : TARGET = pcheck

TEST_TYPE = check
TEST_BUILD = all
TEST_FS = all
TEST_TIME = 3m

all test cstyle: $(TEST)

clean clobber: $(TEST)

$(TEST_DEPS):
	$(MAKE) -C $@ $(TARGET)

$(TEST): $(TEST_DEPS)
	$(MAKE) -C $@ $(TARGET)

check: test
	@./RUNTESTS -b $(TEST_BUILD) -t $(TEST_TYPE) -f $(TEST_FS) -o $(TEST_TIME)
	@echo "No failures."

pcheck: $(TEST)
	@echo "No failures."

.PHONY: all check clean clobber cstyle pcheck test unittest $(TEST) $(TEST_DEPS)
