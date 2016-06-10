#
# Copyright 2014-2016, Intel Corporation
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
# src/test/Makefile -- build all unit tests
#
# Makefile -- build all unit tests
#

include ../common.inc

TEST_DEPS = \
	unittest\
	tools

BLK_TESTS = \
	blk_nblock\
	blk_non_zero\
	blk_pool\
	blk_pool_lock\
	blk_recovery\
	blk_rw\
	blk_rw_mt
LOG_TESTS = \
	log_basic\
	log_pool\
	log_pool_lock\
	log_recovery\
	log_walker

OBJ_DEPS = obj_list
# long tests first
OBJ_TESTS = \
	obj_basic_integration\
	obj_many_size_allocs\
	obj_realloc\
	obj_sync\
	\
	obj_bucket\
	obj_check\
	obj_convert\
	obj_ctree\
	obj_cuckoo\
	obj_debug\
	obj_direct\
	obj_first_next\
	obj_heap\
	obj_heap_interrupt\
	obj_heap_state\
	obj_lane\
	obj_list_insert\
	obj_list_move\
	obj_list_recovery\
	obj_list_remove\
	obj_list_valgrind\
	obj_list_macro\
	obj_locks\
	obj_memblock\
	obj_memcheck\
	obj_out_of_memory\
	obj_persist_count\
	obj_pmalloc_basic\
	obj_pmalloc_mt\
	obj_pmalloc_oom_mt\
	obj_pool\
	obj_pool_lock\
	obj_pool_lookup\
	obj_pvector\
	obj_recovery\
	obj_recreate\
	obj_redo_log\
	obj_strdup\
	obj_toid\
	obj_tx_alloc\
	obj_tx_add_range\
	obj_tx_lock\
	obj_tx_add_range_direct\
	obj_tx_flow\
	obj_tx_free\
	obj_tx_invalid\
	obj_tx_locks\
	obj_tx_locks_abort\
	obj_tx_realloc\
	obj_tx_strdup\
	obj_constructor

OBJ_CPP_TESTS = \
	obj_cpp_ptr\
	obj_cpp_p_ext\
	obj_cpp_pool\
	obj_cpp_ptr_arith\
	obj_cpp_mutex_posix\
	obj_cpp_shared_mutex_posix\
	obj_cpp_pool_primitives\
	obj_cpp_make_persistent\
	obj_cpp_make_persistent_atomic\
	obj_cpp_make_persistent_array\
	obj_cpp_make_persistent_array_atomic\
	obj_cpp_transaction

CHRONO_TESTS = \
	obj_cpp_mutex\
	obj_cpp_shared_mutex\
	obj_cpp_cond_var\
	obj_cpp_cond_var_posix\
	obj_cpp_timed_mtx\
	obj_cpp_timed_mtx_posix

OTHER_TESTS = \
	arch_flags\
	checksum\
	magic\
	out_err\
	out_err_mt\
	pmemobjcli\
	pmemspoil\
	bttdevice\
	remote_basic\
	remote_obj_basic\
	scope\
	set_funcs\
	traces\
	traces_custom_function\
	traces_pmem\
	util_cpuid\
	util_uuid_generate\
	util_file_create\
	util_file_open\
	util_map_proc\
	util_parse_size\
	util_poolset\
	util_poolset_parse\
	util_is_absolute \
	util_is_poolset\
	util_poolset_size\
	util_poolset_foreach

PMEM_TESTS = \
	pmem_is_pmem\
	pmem_is_pmem_proc\
	pmem_map\
	pmem_memcpy\
	pmem_memmove\
	pmem_memset\
	pmem_movnt\
	pmem_movnt_align\
	pmem_valgr_simple

PMEMPOOL_TESTS = \
	pmempool_check\
	pmempool_create\
	pmempool_dump\
	pmempool_help\
	pmempool_info\
	pmempool_rm

RPMEM_TESTS =\
	rpmemd_config\
	rpmemd_log\
	rpmemd_dbg\
	rpmem_proto\
	rpmem_obc\
	rpmemd_obc\
	rpmem_obc_int\
	rpmemd_db\
	rpmem_fip

VMEM_TESTS = \
	vmem_aligned_alloc\
	vmem_calloc\
	vmem_check_allocations\
	vmem_check_version\
	vmem_check\
	vmem_create\
	vmem_create_error\
	vmem_create_in_region\
	vmem_custom_alloc\
	vmem_delete\
	vmem_malloc\
	vmem_malloc_usable_size\
	vmem_mix_allocations\
	vmem_multiple_pools\
	vmem_out_of_memory\
	vmem_pages_purging\
	vmem_realloc\
	vmem_realloc_inplace\
	vmem_stats\
	vmem_strdup\
	vmem_valgrind

VMMALLOC_TESTS = \
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
	vmmalloc_valloc

EXAMPLES_TESTS = \
	ex_libpmem\
	ex_libpmemblk\
	ex_libpmemlog\
	ex_libpmemobj\
	ex_linkedlist

CPP_EXAMPLE_TESTS = \
	ex_libpmemobj_cpp

REMOTE_TESTS = \
	remote_basic\
	remote_obj_basic\
	$(RPMEM_TESTS)

LIBPMEMPOOL_DEPS = libpmempool_api

LIBPMEMPOOL_TESTS = \
	libpmempool_bttdev\
	libpmempool_map_flog

TESTS = \
	$(OBJ_TESTS)\
	$(BLK_TESTS)\
	$(LOG_TESTS)\
	$(OTHER_TESTS)\
	$(PMEM_TESTS)\
	$(RPMEM_TESTS)\
	$(PMEMPOOL_TESTS)\
	$(VMEM_TESTS)\
	$(VMMALLOC_TESTS)\
	$(EXAMPLES_TESTS)\
	$(LIBPMEMPOOL_TESTS)

ifeq ($(filter n,$(call check_cxx_flags, -std=c++11) $(call check_clang_template_bug)),)
TESTS += $(OBJ_CPP_TESTS)

ifeq ($(call cxx_ok), y)
TESTS += $(CPP_EXAMPLE_TESTS)
else
$(info NOTE: Skipping C++ tests because of compiler issues)
endif

ifeq ($(call check_cxx_chrono), y)
TESTS += $(CHRONO_TESTS)
else
$(info NOTE: Skipping C++ chrono tests because of compiler/stdc++ issues)
endif
else
$(info NOTE: Skipping C++ tests because the compiler does not support C++11)
endif

TESTS_BUILD = \
	$(TEST_DEPS)\
	$(OBJ_DEPS)\
	$(LIBPMEMPOOL_DEPS)\
	$(TESTS)

all     : TARGET = all
clean   : TARGET = clean
clobber : TARGET = clobber
test    : TARGET = test
cstyle  : TARGET = cstyle
format  : TARGET = format
check   : TARGET = check
pcheck  : TARGET = pcheck

TEST_TYPE = check
TEST_BUILD = all
TEST_FS = all
TEST_TIME = 3m
MEMCHECK = auto
CHECK_POOL =

ifeq ($(CHECK_POOL),y)
CHECK_POOL_OPT="-c"
endif

all test cstyle format: $(TESTS_BUILD)

clean clobber: $(TESTS_BUILD)

$(TESTS) $(OBJ_DEPS) $(LIBPMEMPOOL_DEPS): $(TEST_DEPS)

$(OBJ_TESTS): $(OBJ_DEPS)
$(LIBPMEMPOOL_TESTS): $(LIBPMEMPOOL_DEPS)

$(TESTS_BUILD):
	$(MAKE) -C $@ $(TARGET)

memcheck-summary:
	grep ERROR */memcheck*.log

memcheck-summary-errors:
	grep ERROR */memcheck*.log | grep -v " 0 errors" || true

memcheck-summary-leaks:
	grep "in use at exit" */memcheck*.log | grep -v " 0 bytes in 0 blocks" || true

check: test
	@./RUNTESTS -b $(TEST_BUILD) -t $(TEST_TYPE) -f $(TEST_FS) -o $(TEST_TIME) -m $(MEMCHECK) $(CHECK_POOL_OPT) $(TESTS)
	@echo "No failures."

check-remote: test
	@./RUNTESTS -b $(TEST_BUILD) -t $(TEST_TYPE) -f $(TEST_FS) -o $(TEST_TIME) -m $(MEMCHECK) $(CHECK_POOL_OPT) $(REMOTE_TESTS)
	@echo "No failures."

pcheck: $(TESTS_BUILD)
	@echo "No failures."

pcheck_blk: TARGET = pcheck
pcheck_blk: $(BLK_TESTS)
	@echo "No failures."

pcheck_log: TARGET = pcheck
pcheck_log: $(LOG_TESTS)
	@echo "No failures."

pcheck_obj: TARGET = pcheck
pcheck_obj: $(OBJ_TESTS)
	@echo "No failures."

pcheck_other: TARGET = pcheck
pcheck_other: $(OTHER_TESTS)
	@echo "No failures."

pcheck_pmem: TARGET = pcheck
pcheck_pmem: $(PMEM_TESTS)
	@echo "No failures."

pcheck_rpmem: TARGET = pcheck
pcheck_rpmem: $(RPMEM_TESTS)
	@echo "No failures."

pcheck_pmempool: TARGET = pcheck
pcheck_pmempool: $(PMEMPOOL_TESTS)
	@echo "No failures."

pcheck_libpmempool: TARGET = pcheck
pcheck_libpmempool: $(LIBPMEMPOOL_TESTS)
	@echo "No failures."

pcheck_vmem: TARGET = pcheck
pcheck_vmem: $(VMEM_TESTS)
	@echo "No failures."

pcheck_vmmalloc: TARGET = pcheck
pcheck_vmmalloc: $(VMMALLOC_TESTS)
	@echo "No failures."

.PHONY: all check clean clobber cstyle pcheck pcheck_blk pcheck_log pcheck_obj\
	 pcheck_other pcheck_pmem pcheck_pmempool pcheck_vmem pcheck_vmmalloc\
	 test unittest tools check-remote format pcheck_libpmempool\
	 $(TESTS_BUILD)
