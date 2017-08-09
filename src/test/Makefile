#
# Copyright 2014-2017, Intel Corporation
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
	obj_async_postcommit\
	obj_basic_integration\
	obj_many_size_allocs\
	obj_realloc\
	obj_sync\
	\
	obj_bucket\
	obj_check\
	obj_convert\
	obj_ctl\
	obj_ctl_alloc_class\
	obj_ctl_alloc_class_config\
	obj_ctl_config\
	obj_ctl_prefault\
	obj_ctree\
	obj_cuckoo\
	obj_debug\
	obj_direct\
	obj_first_next\
	obj_fragmentation\
	obj_heap\
	obj_heap_interrupt\
	obj_heap_state\
	obj_include\
	obj_lane\
	obj_layout\
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
	obj_pmalloc_rand_mt\
	obj_pool\
	obj_pool_lock\
	obj_pool_lookup\
	obj_pvector\
	obj_recovery\
	obj_recreate\
	obj_redo_log\
	obj_ringbuf\
	obj_strdup\
	obj_toid\
	obj_tx_alloc\
	obj_tx_add_range\
	obj_tx_callbacks\
	obj_tx_lock\
	obj_tx_add_range_direct\
	obj_tx_flow\
	obj_tx_free\
	obj_tx_invalid\
	obj_tx_locks\
	obj_tx_locks_abort\
	obj_tx_mt\
	obj_tx_realloc\
	obj_tx_strdup\
	obj_constructor\
	obj_oid\
	obj_zones

OBJ_REMOTE_TESTS = \
	obj_rpmem_basic_integration\
	obj_rpmem_heap_state\
	obj_rpmem_heap_interrupt

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
	obj_cpp_transaction\
	obj_cpp_allocator

OBJ_CPP_CONTAINER_TESTS = \
	obj_cpp_vector\
	obj_cpp_deque\
	obj_cpp_list\
	obj_cpp_set\
	obj_cpp_multiset\
	obj_cpp_unordered_multiset\
	obj_cpp_unordered_set\
	obj_cpp_map\
	obj_cpp_multimap\
	obj_cpp_unordered_map\
	obj_cpp_unordered_multimap\
	obj_cpp_queue\
	obj_cpp_priority_queue\
	obj_cpp_stack

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
	scope\
	set_funcs\
	traces\
	traces_custom_function\
	traces_pmem\
	unicode_api\
	unicode_match_script\
	util_cpuid\
	util_uuid_generate\
	util_file_create\
	util_file_open\
	util_map_proc\
	util_parse_size\
	util_plugin\
	util_poolset\
	util_poolset_parse\
	util_is_absolute\
	util_is_poolset\
	util_poolset_size\
	util_poolset_foreach

PMEM_TESTS = \
	pmem_is_pmem\
	pmem_is_pmem_linux\
	pmem_map_file\
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
	pmempool_rm\
	pmempool_sync\
	pmempool_transform

RPMEM_TESTS =\
	rpmemd_config\
	rpmemd_util\
	rpmemd_log\
	rpmemd_dbg\
	rpmem_proto\
	rpmem_obc\
	rpmemd_obc\
	rpmem_obc_int\
	rpmemd_db\
	rpmem_fip\
	rpmem_basic\
	rpmem_addr

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

LIBPMEMPOOL_DEPS = libpmempool_api

LIBPMEMPOOL_TESTS = \
	libpmempool_backup\
	libpmempool_bttdev\
	libpmempool_map_flog\
	libpmempool_rm

LIBPMEMPOOL_MOD_DEPS = \
	libpmempool_sync
LIBPMEMPOOL_MOD_TESTS = \
	libpmempool_transform


LOCAL_TESTS = \
	$(OBJ_TESTS)\
	$(BLK_TESTS)\
	$(LOG_TESTS)\
	$(OTHER_TESTS)\
	$(PMEM_TESTS)\
	$(PMEMPOOL_TESTS)\
	$(VMEM_TESTS)\
	$(VMMALLOC_TESTS)\
	$(EXAMPLES_TESTS)\
	$(LIBPMEMPOOL_TESTS)\
	$(LIBPMEMPOOL_MOD_TESTS)

REMOTE_TESTS = \
	remote_basic\
	remote_obj_basic\
	$(OBJ_REMOTE_TESTS)\
	$(RPMEM_TESTS)\
	pmempool_info_remote\
	pmempool_sync_remote\
	pmempool_transform_remote\
	libpmempool_rm_remote

ifeq ($(filter n,$(call check_cxx_flags, -std=c++11) $(call check_clang_template_bug)),)
LOCAL_TESTS += $(OBJ_CPP_TESTS)

ifeq ($(call cxx_ok), y)
LOCAL_TESTS += $(CPP_EXAMPLE_TESTS)
else
$(info NOTE: Skipping C++ tests because of compiler issues)
endif

ifeq ($(call check_cxx_chrono), y)
LOCAL_TESTS += $(CHRONO_TESTS)
else
$(info NOTE: Skipping C++ chrono tests because of compiler/stdc++ issues)
endif
else
$(info NOTE: Skipping C++ tests because the compiler does not support C++11)
endif

# enable only when using custom standard libraries
ifneq (,$(filter 1, $(USE_CUSTOM_GCC) $(USE_LLVM_LIBCPP)))
LOCAL_TESTS += $(OBJ_CPP_CONTAINER_TESTS)
else
$(info NOTE: Skipping C++ container tests because a custom version of the standard library is not used)
endif

TESTS = \
	$(LOCAL_TESTS)\
	$(REMOTE_TESTS)

TESTS_BUILD = \
	$(TEST_DEPS)\
	$(OBJ_DEPS)\
	$(LIBPMEMPOOL_DEPS)\
	$(LIBPMEMPOOL_MOD_DEPS)\
	$(TESTS)

all     : TARGET = all
clean   : TARGET = clean
clobber : TARGET = clobber
test    : TARGET = test
cstyle  : TARGET = cstyle
format  : TARGET = format
check   : TARGET = check
pcheck  : TARGET = pcheck
sparse  : TARGET = sparse

DIR_SYNC=$(TOP)/src/test/.sync-dir
SYNC_EXT=synced

all test cstyle format sparse: $(TESTS_BUILD)

clean clobber: $(TESTS_BUILD)
	$(RM) -r $(DIR_SYNC)
	$(RM) *.$(SYNC_EXT)

$(TESTS) $(OBJ_DEPS) $(LIBPMEMPOOL_DEPS) $(LIBPMEMPOOL_MOD_DEPS): $(TEST_DEPS)

$(OBJ_TESTS): $(OBJ_DEPS)
$(LIBPMEMPOOL_TESTS): $(LIBPMEMPOOL_DEPS)
$(LIBPMEMPOOL_MOD_TESTS): $(LIBPMEMPOOL_MOD_DEPS)

$(TESTS_BUILD):
	$(MAKE) -C $@ $(TARGET)

memcheck-summary:
	grep ERROR */memcheck*.log

memcheck-summary-errors:
	grep ERROR */memcheck*.log | grep -v " 0 errors" || true

memcheck-summary-leaks:
	grep "in use at exit" */memcheck*.log | grep -v " 0 bytes in 0 blocks" || true

check: test
	@./RUNTESTS $(RUNTEST_OPTIONS) $(TESTS)
	@echo "No failures."

check-remote-quiet: sync-remotes
	@./RUNTESTS $(RUNTEST_OPTIONS) $(REMOTE_TESTS) MAKEFLAGS="$MAKEFLAGS"

sync-remotes: test

check-remote: check-remote-quiet
	@echo "No failures."

# XXX remote tests do not work in parallel mode
pcheck: pcheck-local-quiet check-remote-quiet
	@echo "No failures."

pcheck-blk: TARGET = pcheck
pcheck-blk: $(BLK_TESTS)
	@echo "No failures."

pcheck-log: TARGET = pcheck
pcheck-log: $(LOG_TESTS)
	@echo "No failures."

pcheck-obj: TARGET = pcheck
pcheck-obj: $(OBJ_TESTS)
	@echo "No failures."

pcheck-other: TARGET = pcheck
pcheck-other: $(OTHER_TESTS)
	@echo "No failures."

pcheck-pmem: TARGET = pcheck
pcheck-pmem: $(PMEM_TESTS)
	@echo "No failures."

pcheck-rpmem: TARGET = pcheck
pcheck-rpmem: $(RPMEM_TESTS)
	@echo "No failures."

pcheck-pmempool: TARGET = pcheck
pcheck-pmempool: $(PMEMPOOL_TESTS)
	@echo "No failures."

pcheck-libpmempool: TARGET = pcheck
pcheck-libpmempool: $(LIBPMEMPOOL_TESTS)
	@echo "No failures."

pcheck-vmem: TARGET = pcheck
pcheck-vmem: $(VMEM_TESTS)
	@echo "No failures."

pcheck-vmmalloc: TARGET = pcheck
pcheck-vmmalloc: $(VMMALLOC_TESTS)
	@echo "No failures."

pcheck-local-quiet: TARGET = pcheck
pcheck-local-quiet: $(LOCAL_TESTS)

pcheck-local: pcheck-local-quiet
	@echo "No failures."

pcheck-remote: TARGET = pcheck
pcheck-remote: $(REMOTE_TESTS)
	@echo "No failures."

TESTCONFIG=$(TOP)/src/test/testconfig.sh

$(TESTCONFIG):

SUPP_SYNC_FILES=$(shell echo *.supp | sed s/supp/$(SYNC_EXT)/g)

%.$(SYNC_EXT): %.supp $(TESTCONFIG)
	cp $(shell echo $^ | cut -d" " -f1) $(DIR_SYNC)
	@touch $@

# sync remote nodes
sync-remotes:
ifeq ($(FORCE_SYNC_REMOTES),y)
	@touch $(TESTCONFIG)
endif
ifneq ($(SKIP_SYNC_REMOTES),y)
	@rm -rf $(DIR_SYNC) && mkdir -p $(DIR_SYNC)
	$(MAKE) $(SUPP_SYNC_FILES)
	$(MAKE) -C tools $@
	$(MAKE) -C ../tools $@
	$(MAKE) -C sync-remotes $@
	@for test in $(REMOTE_TESTS); do \
		echo "$(MAKE) -C $$test sync-test" ; \
		$(MAKE) -C $$test sync-test || exit 1 ; \
	done
	@rm -rf $(DIR_SYNC)
endif

.PHONY: all check clean clobber cstyle pcheck pcheck-blk pcheck-log pcheck-obj\
	 pcheck-other pcheck-pmem pcheck-pmempool pcheck-vmem pcheck-vmmalloc\
	 test unittest tools check-remote format pcheck-libpmempool\
	 pcheck-rpmem pcheck-local pcheck-remote sync-remotes $(TESTS_BUILD)
