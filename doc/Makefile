# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation
#

#
# doc/Makefile -- Makefile for PMDK man pages
#

include ../src/common.inc

MANPAGES_7_MD = libpmem/libpmem.7.md libpmemblk/libpmemblk.7.md libpmemlog/libpmemlog.7.md \
		libpmemobj/libpmemobj.7.md libpmempool/libpmempool.7.md

MANPAGES_5_MD = poolset/poolset.5.md pmem_ctl/pmem_ctl.5.md

MANPAGES_3_MD = libpmem/pmem_flush.3.md libpmem/pmem_is_pmem.3.md libpmem/pmem_memmove_persist.3.md \
		libpmemblk/pmemblk_bsize.3.md libpmemblk/pmemblk_create.3.md libpmemblk/pmemblk_ctl_get.3.md libpmemblk/pmemblk_read.3.md libpmemblk/pmemblk_set_zero.3.md \
		libpmemlog/pmemlog_append.3.md libpmemlog/pmemlog_create.3.md libpmemlog/pmemlog_ctl_get.3.md libpmemlog/pmemlog_nbyte.3.md libpmemlog/pmemlog_tell.3.md \
		libpmemobj/oid_is_null.3.md libpmemobj/pmemobj_action.3.md libpmemobj/pmemobj_alloc.3.md libpmemobj/pmemobj_ctl_get.3.md libpmemobj/pmemobj_first.3.md \
		libpmemobj/pmemobj_list_insert.3.md libpmemobj/pmemobj_memcpy_persist.3.md libpmemobj/pmemobj_mutex_zero.3.md \
		libpmemobj/pmemobj_open.3.md libpmemobj/pmemobj_root.3.md libpmemobj/pmemobj_tx_begin.3.md libpmemobj/pmemobj_tx_add_range.3.md \
		libpmemobj/pmemobj_tx_alloc.3.md libpmemobj/pobj_layout_begin.3.md libpmemobj/pobj_list_head.3.md libpmemobj/toid_declare.3.md \
		libpmempool/pmempool_check_init.3.md libpmempool/pmempool_feature_query.3.md libpmempool/pmempool_rm.3.md libpmempool/pmempool_sync.3.md

MANPAGES_1_MD = pmempool/pmempool.1.md pmempool/pmempool-info.1.md pmempool/pmempool-create.1.md \
		pmempool/pmempool-check.1.md pmempool/pmempool-dump.1.md pmempool/pmempool-rm.1.md \
		pmempool/pmempool-convert.1.md pmempool/pmempool-sync.1.md pmempool/pmempool-transform.1.md \
		pmempool/pmempool-feature.1.md pmreorder/pmreorder.1.md

MANPAGES_3_DUMMY = libpmem/pmem_drain.3 libpmem/pmem_has_hw_drain.3 libpmem/pmem_has_auto_flush.3 \
		   libpmem/pmem_persist.3 libpmem/pmem_msync.3 libpmem/pmem_map_file.3 libpmem/pmem_deep_persist.3 libpmem/pmem_deep_flush.3 libpmem/pmem_deep_drain.3 libpmem/pmem_unmap.3 \
		   libpmem/pmem_memcpy_persist.3 libpmem/pmem_memset_persist.3 libpmem/pmem_memmove_nodrain.3 libpmem/pmem_memcpy_nodrain.3 libpmem/pmem_memset_nodrain.3 \
		   libpmem/pmem_memcpy.3 libpmem/pmem_memset.3 libpmem/pmem_memmove.3 \
		   libpmem/pmem_check_version.3 libpmem/pmem_errormsg.3 \
		   libpmemblk/pmemblk_nblock.3 \
		   libpmemblk/pmemblk_open.3 libpmemblk/pmemblk_close.3 \
		   libpmemblk/pmemblk_write.3 \
		   libpmemblk/pmemblk_set_error.3 \
		   libpmemblk/pmemblk_check_version.3 libpmemblk/pmemblk_check.3 libpmemblk/pmemblk_errormsg.3 libpmemblk/pmemblk_set_funcs.3 \
		   libpmemblk/pmemblk_ctl_set.3 libpmemblk/pmemblk_ctl_exec.3\
		   libpmemlog/pmemlog_rewind.3 libpmemlog/pmemlog_walk.3 \
		   libpmemlog/pmemlog_open.3 libpmemlog/pmemlog_close.3 \
		   libpmemlog/pmemlog_appendv.3 \
		   libpmemlog/pmemlog_check_version.3 libpmemlog/pmemlog_check.3 libpmemlog/pmemlog_errormsg.3 libpmemlog/pmemlog_set_funcs.3 \
		   libpmemlog/pmemlog_ctl_set.3 libpmemlog/pmemlog_ctl_exec.3\
		   libpmempool/pmempool_check.3 libpmempool/pmempool_check_end.3 \
		   libpmempool/pmempool_feature_enable.3 libpmempool/pmempool_feature_disable.3 \
		   libpmempool/pmempool_transform.3 \
		   libpmempool/pmempool_check_version.3 libpmempool/pmempool_errormsg.3 \
		   libpmemobj/oid_equals.3 libpmemobj/pmemobj_direct.3 libpmemobj/pmemobj_oid.3 libpmemobj/pmemobj_type_num.3 libpmemobj/pmemobj_pool_by_oid.3 libpmemobj/pmemobj_pool_by_ptr.3 libpmemobj/pmemobj_volatile.3\
		   libpmemobj/pmemobj_zalloc.3 libpmemobj/pmemobj_xalloc.3 libpmemobj/pmemobj_free.3 libpmemobj/pmemobj_realloc.3 libpmemobj/pmemobj_zrealloc.3 libpmemobj/pmemobj_strdup.3 libpmemobj/pmemobj_wcsdup.3 libpmemobj/pmemobj_alloc_usable_size.3 \
		   libpmemobj/pobj_new.3 libpmemobj/pobj_alloc.3 libpmemobj/pobj_znew.3 libpmemobj/pobj_zalloc.3 libpmemobj/pobj_realloc.3 libpmemobj/pobj_zrealloc.3 libpmemobj/pobj_free.3 \
		   libpmemobj/pobj_layout_toid.3 libpmemobj/pobj_layout_root.3 libpmemobj/pobj_layout_name.3 libpmemobj/pobj_layout_end.3 libpmemobj/pobj_layout_types_num.3 \
		   libpmemobj/pmemobj_ctl_set.3 libpmemobj/pmemobj_ctl_exec.3\
		   libpmemobj/pmemobj_create.3 libpmemobj/pmemobj_close.3 \
		   libpmemobj/pmemobj_list_insert_new.3 libpmemobj/pmemobj_list_remove.3 libpmemobj/pmemobj_list_move.3 \
		   libpmemobj/toid_declare_root.3 libpmemobj/toid.3 libpmemobj/toid_type_num.3 libpmemobj/toid_type_num_of.3 libpmemobj/toid_valid.3 libpmemobj/oid_instanceof.3 libpmemobj/toid_assign.3 libpmemobj/toid_is_null.3 libpmemobj/toid_equals.3 libpmemobj/toid_typeof.3 libpmemobj/toid_offsetof.3 libpmemobj/direct_rw.3 libpmemobj/d_rw.3 libpmemobj/direct_ro.3 libpmemobj/d_ro.3 \
		   libpmemobj/pmemobj_memcpy.3 libpmemobj/pmemobj_memmove.3 libpmemobj/pmemobj_memset.3 \
		   libpmemobj/pmemobj_memset_persist.3 libpmemobj/pmemobj_persist.3 libpmemobj/pmemobj_xpersist.3 libpmemobj/pmemobj_flush.3 libpmemobj/pmemobj_xflush.3 libpmemobj/pmemobj_drain.3 \
		   libpmemobj/pmemobj_tx_stage.3 libpmemobj/pmemobj_tx_lock.3 libpmemobj/pmemobj_tx_xlock.3 libpmemobj/pmemobj_tx_abort.3 libpmemobj/pmemobj_tx_commit.3 libpmemobj/pmemobj_tx_end.3 libpmemobj/pmemobj_tx_errno.3 \
		   libpmemobj/pmemobj_tx_process.3 libpmemobj/pmemobj_tx_add_range_direct.3 libpmemobj/pmemobj_tx_xadd_range.3 libpmemobj/pmemobj_tx_xadd_range_direct.3 \
		   libpmemobj/pmemobj_tx_zalloc.3 libpmemobj/pmemobj_tx_xalloc.3 libpmemobj/pmemobj_tx_realloc.3 libpmemobj/pmemobj_tx_zrealloc.3 libpmemobj/pmemobj_tx_strdup.3 libpmemobj/pmemobj_tx_xstrdup.3 libpmemobj/pmemobj_tx_wcsdup.3 libpmemobj/pmemobj_tx_xwcsdup.3 libpmemobj/pmemobj_tx_free.3 libpmemobj/pmemobj_tx_xfree.3\
		   libpmemobj/pmemobj_tx_log_append_buffer.3 libpmemobj/pmemobj_tx_xlog_append_buffer.3 libpmemobj/pmemobj_tx_log_auto_alloc.3 libpmemobj/pmemobj_tx_log_snapshots_max_size.3 libpmemobj/pmemobj_tx_log_intents_max_size.3 \
		   libpmemobj/tx_begin_param.3 libpmemobj/tx_begin_cb.3 libpmemobj/tx_begin.3 libpmemobj/tx_onabort.3 libpmemobj/tx_oncommit.3 libpmemobj/tx_finally.3 libpmemobj/tx_end.3 \
		   libpmemobj/tx_add.3 libpmemobj/tx_add_field.3 libpmemobj/tx_add_direct.3 libpmemobj/tx_add_field_direct.3 libpmemobj/tx_xadd.3 libpmemobj/tx_xadd_field.3 libpmemobj/tx_xadd_direct.3 libpmemobj/tx_xadd_field_direct.3 \
		   libpmemobj/tx_new.3 libpmemobj/tx_alloc.3 libpmemobj/tx_znew.3 libpmemobj/tx_zalloc.3 libpmemobj/tx_xalloc.3 libpmemobj/tx_realloc.3 libpmemobj/tx_zrealloc.3 libpmemobj/tx_strdup.3 libpmemobj/tx_wcsdup.3 libpmemobj/tx_free.3 libpmemobj/tx_set.3 libpmemobj/tx_set_direct.3 libpmemobj/tx_memcpy.3 libpmemobj/tx_memset.3 \
		   libpmemobj/pmemobj_f_mem_nodrain.3 libpmemobj/pmemobj_f_mem_nontemporal.3 libpmemobj/pmemobj_f_mem_temporal.3 libpmemobj/pmemobj_f_mem_wc.3 libpmemobj/pmemobj_f_mem_wb.3 libpmemobj/pmemobj_f_mem_noflush.3 libpmemobj/pmemobj_f_relaxed.3 \
		   libpmemobj/pmemobj_mutex_lock.3 libpmemobj/pmemobj_mutex_timedlock.3 libpmemobj/pmemobj_mutex_trylock.3 libpmemobj/pmemobj_mutex_unlock.3 \
		   libpmemobj/pmemobj_rwlock_zero.3 libpmemobj/pmemobj_rwlock_rdlock.3 libpmemobj/pmemobj_rwlock_wrlock.3 libpmemobj/pmemobj_rwlock_timedrdlock.3 libpmemobj/pmemobj_rwlock_timedwrlock.3 libpmemobj/pmemobj_rwlock_tryrdlock.3 libpmemobj/pmemobj_rwlock_trywrlock.3 libpmemobj/pmemobj_rwlock_unlock.3 \
		   libpmemobj/pmemobj_cond_zero.3 libpmemobj/pmemobj_cond_broadcast.3 libpmemobj/pmemobj_cond_signal.3 libpmemobj/pmemobj_cond_timedwait.3 libpmemobj/pmemobj_cond_wait.3 \
		   libpmemobj/pobj_list_entry.3 libpmemobj/pobj_list_first.3 libpmemobj/pobj_list_last.3 libpmemobj/pobj_list_empty.3 libpmemobj/pobj_list_next.3 libpmemobj/pobj_list_prev.3 libpmemobj/pobj_list_foreach.3 libpmemobj/pobj_list_foreach_reverse.3 \
		   libpmemobj/pobj_list_insert_head.3 libpmemobj/pobj_list_insert_tail.3 libpmemobj/pobj_list_insert_after.3 libpmemobj/pobj_list_insert_before.3 libpmemobj/pobj_list_insert_new_head.3 libpmemobj/pobj_list_insert_new_tail.3 \
		   libpmemobj/pobj_list_insert_new_after.3 libpmemobj/pobj_list_insert_new_before.3 libpmemobj/pobj_list_remove.3 libpmemobj/pobj_list_remove_free.3 \
		   libpmemobj/pobj_list_move_element_head.3 libpmemobj/pobj_list_move_element_tail.3 libpmemobj/pobj_list_move_element_after.3 libpmemobj/pobj_list_move_element_before.3 \
		   libpmemobj/pmemobj_next.3 libpmemobj/pobj_first_type_num.3 libpmemobj/pobj_first.3 libpmemobj/pobj_next_type_num.3 libpmemobj/pobj_next.3 libpmemobj/pobj_foreach.3 libpmemobj/pobj_foreach_safe.3 libpmemobj/pobj_foreach_type.3 libpmemobj/pobj_foreach_safe_type.3 \
		   libpmemobj/pmemobj_root_construct.3 libpmemobj/pobj_root.3 libpmemobj/pmemobj_root_size.3 \
		   libpmemobj/pmemobj_check_version.3 libpmemobj/pmemobj_check.3 libpmemobj/pmemobj_errormsg.3 libpmemobj/pmemobj_set_funcs.3 \
		   libpmemobj/pmemobj_reserve.3 libpmemobj/pmemobj_xreserve.3 libpmemobj/pmemobj_defer_free.3 libpmemobj/pmemobj_set_value.3 libpmemobj/pmemobj_publish.3 libpmemobj/pmemobj_tx_publish.3 libpmemobj/pmemobj_tx_xpublish.3 libpmemobj/pmemobj_cancel.3 libpmemobj/pobj_reserve_new.3 libpmemobj/pobj_reserve_alloc.3 libpmemobj/pobj_xreserve_new.3 libpmemobj/pobj_xreserve_alloc.3 \
		   libpmemobj/tx_xstrdup.3 libpmemobj/tx_xwcsdup.3 libpmemobj/tx_xfree.3 \
		   libpmemobj/pmemobj_defrag.3 libpmemobj/pmemobj_get_user_data.3 libpmemobj/pmemobj_set_user_data.3 libpmemobj/pmemobj_tx_get_user_data.3 libpmemobj/pmemobj_tx_set_user_data.3 libpmemobj/pmemobj_tx_get_failure_behavior.3 libpmemobj/pmemobj_tx_set_failure_behavior.3

MANPAGES_WEBDIR_LINUX = web_linux
MANPAGES_WEBDIR_WINDOWS = web_windows

# experimental
MANPAGES_7_MD_EXP =
MANPAGES_5_MD_EXP =
MANPAGES_3_MD_EXP =
MANPAGES_1_MD_EXP =

MANPAGES_3_DUMMY_EXP =
MANPAGES_1_DUMMY_EXP =

# libpmem2
MANPAGES_7_MD_PMEM2 = libpmem2/libpmem2.7.md libpmem2/libpmem2_unsafe_shutdown.7.md
MANPAGES_5_MD_PMEM2 =
MANPAGES_3_MD_PMEM2 = libpmem2/pmem2_errormsg.3.md libpmem2/pmem2_config_new.3.md libpmem2/pmem2_map_new.3.md \
		libpmem2/pmem2_map_delete.3.md libpmem2/pmem2_map_get_address.3.md libpmem2/pmem2_map_get_size.3.md \
		libpmem2/pmem2_source_from_fd.3.md libpmem2/pmem2_config_set_required_store_granularity.3.md \
		libpmem2/pmem2_source_size.3.md libpmem2/pmem2_source_alignment.3.md libpmem2/pmem2_source_numa_node.3.md \
		libpmem2/pmem2_config_set_length.3.md libpmem2/pmem2_config_set_offset.3.md \
		libpmem2/pmem2_map_get_store_granularity.3.md libpmem2/pmem2_get_flush_fn.3.md \
		libpmem2/pmem2_get_drain_fn.3.md libpmem2/pmem2_get_persist_fn.3.md \
		libpmem2/pmem2_perror.3.md libpmem2/pmem2_get_memmove_fn.3.md libpmem2/pmem2_async.3.md\
		libpmem2/pmem2_config_set_sharing.3.md libpmem2/pmem2_config_set_vm_reservation.3.md libpmem2/pmem2_vm_reservation_new.3.md \
		libpmem2/pmem2_vm_reservation_get_address.3.md libpmem2/pmem2_vm_reservation_get_size.3.md \
		libpmem2/pmem2_badblock_context_new.3.md libpmem2/pmem2_badblock_next.3.md \
		libpmem2/pmem2_badblock_clear.3.md libpmem2/pmem2_config_set_protection.3.md \
		libpmem2/pmem2_deep_flush.3.md libpmem2/pmem2_source_from_anon.3.md \
		libpmem2/pmem2_source_device_id.3.md libpmem2/pmem2_source_device_usc.3.md \
		libpmem2/pmem2_map_from_existing.3.md libpmem2/pmem2_source_get_fd.3.md \
		libpmem2/pmem2_source_get_handle.3.md libpmem2/pmem2_vm_reservation_extend.3.md \
		libpmem2/pmem2_vm_reservation_map_find.3.md libpmem2/pmem2_source_pread_mcsafe.3.md \

MANPAGES_1_MD_PMEM2 =
MANPAGES_3_DUMMY += libpmem2/pmem2_config_delete.3 libpmem2/pmem2_config_set_vdm.3 libpmem2/pmem2_source_from_handle.3 libpmem2/pmem2_source_delete.3 \
	libpmem2/pmem2_get_memset_fn.3 libpmem2/pmem2_get_memcpy_fn.3 libpmem2/pmem2_memcpy_async.3 libpmem2/pmem2_memmove_async.3 \
	libpmem2/pmem2_memset_async.3 libpmem2/pmem2_vm_reservation_delete.3 \
	libpmem2/pmem2_badblock_context_delete.3 libpmem2/pmem2_vm_reservation_shrink.3 \
	libpmem2/pmem2_vm_reservation_map_find_first.3 libpmem2/pmem2_vm_reservation_map_find_last.3 \
	libpmem2/pmem2_vm_reservation_map_find_next.3 libpmem2/pmem2_vm_reservation_map_find_prev.3 \
	libpmem2/pmem2_source_pwrite_mcsafe.3

# libpmemset
MANPAGES_7_MD_PMEMSET = libpmemset/libpmemset.7.md
MANPAGES_5_MD_PMEMSET =
MANPAGES_3_MD_PMEMSET = libpmemset/pmemset_errormsg.3.md libpmemset/pmemset_perror.3.md libpmemset/pmemset_config_new.3.md \
			libpmemset/pmemset_source_from_pmem2.3.md libpmemset/pmemset_new.3.md \
			libpmemset/pmemset_source_from_file.3.md libpmemset/pmemset_source_delete.3.md libpmemset/pmemset_map_config_new.3.md \
			libpmemset/pmemset_config_set_required_store_granularity.3.md libpmemset/pmemset_map.3.md \
			libpmemset/pmemset_first_part_map.3.md libpmemset/pmemset_descriptor_part_map.3.md libpmemset/pmemset_get_store_granularity.3.md \
			libpmemset/pmemset_part_map_drop.3.md libpmemset/pmemset_part_map_by_address.3.md \
			libpmemset/pmemset_persist.3.md libpmemset/pmemset_flush.3.md libpmemset/pmemset_drain.3.md \
			libpmemset/pmemset_set_contiguous_part_coalescing.3.md libpmemset/pmemset_memmove.3.md \
			libpmemset/pmemset_remove_part_map.3.md libpmemset/pmemset_deep_flush.3.md \
			libpmemset/pmemset_source_from_temporary.3.md libpmemset/pmemset_remove_range.3.md \
			libpmemset/pmemset_config_set_event_callback.3.md libpmemset/pmemset_config_set_reservation.3.md \
			libpmemset/pmemset_map_config_set_length.3.md libpmemset/pmemset_map_config_set_offset.3.md \
			libpmemset/pmemset_config_set_acceptable_states.3.md libpmemset/pmemset_source_set_sds.3.md \
			libpmemset/pmemset_source_pread_mcsafe.3.md libpmemset/pmemset_source_alignment.3.md

MANPAGES_1_MD_PMEMSET =
ifeq ($(PMEMSET_INSTALL),y)
MANPAGES_3_DUMMY += libpmemset/pmemset_config_delete.3 libpmemset/pmemset_delete.3 \
		    libpmemset/pmemset_next_part_map.3 libpmemset/pmemset_memcpy.3 libpmemset/pmemset_memset.3 \
		    libpmemset/pmemset_xsource_from_file.3 libpmemset/pmemset_map_config_delete.3 \
		    libpmemset/pmemset_source_pwrite_mcsafe.3
endif

ifeq ($(BUILD_RPMEM),y)
MANPAGES_7_MD += librpmem/librpmem.7.md
MANPAGES_3_MD += librpmem/rpmem_create.3.md
MANPAGES_3_MD += librpmem/rpmem_persist.3.md
MANPAGES_1_MD += rpmemd/rpmemd.1.md
MANPAGES_3_DUMMY += librpmem/rpmem_open.3 librpmem/rpmem_set_attr.3 librpmem/rpmem_close.3 \
		    librpmem/rpmem_read.3 librpmem/rpmem_remove.3 librpmem/rpmem_check_version.3 \
		    librpmem/rpmem_errormsg.3 librpmem/rpmem_deep_persist.3 librpmem/rpmem_flush.3 \
		    librpmem/rpmem_drain.3
endif

ifeq ($(NDCTL_ENABLE),y)
MANPAGES_1_MD += daxio/daxio.1.md
endif

MANPAGES_7_GROFF = $(MANPAGES_7_MD:.7.md=.7)
MANPAGES_5_GROFF = $(MANPAGES_5_MD:.5.md=.5)
MANPAGES_3_GROFF = $(MANPAGES_3_MD:.3.md=.3)
MANPAGES_1_GROFF = $(MANPAGES_1_MD:.1.md=.1)

MANPAGES_7_GROFF_EXP = $(MANPAGES_7_MD_EXP:.7.md=.7)
MANPAGES_5_GROFF_EXP = $(MANPAGES_5_MD_EXP:.5.md=.5)
MANPAGES_3_GROFF_EXP = $(MANPAGES_3_MD_EXP:.3.md=.3)
MANPAGES_1_GROFF_EXP = $(MANPAGES_1_MD_EXP:.1.md=.1)

MANPAGES_7_GROFF_PMEM2 = $(MANPAGES_7_MD_PMEM2:.7.md=.7)
MANPAGES_5_GROFF_PMEM2 = $(MANPAGES_5_MD_PMEM2:.5.md=.5)
MANPAGES_3_GROFF_PMEM2 = $(MANPAGES_3_MD_PMEM2:.3.md=.3)
MANPAGES_1_GROFF_PMEM2 = $(MANPAGES_1_MD_PMEM2:.1.md=.1)

MANPAGES_7_GROFF_PMEMSET = $(MANPAGES_7_MD_PMEMSET:.7.md=.7)
MANPAGES_5_GROFF_PMEMSET = $(MANPAGES_5_MD_PMEMSET:.5.md=.5)
MANPAGES_3_GROFF_PMEMSET = $(MANPAGES_3_MD_PMEMSET:.3.md=.3)
MANPAGES_1_GROFF_PMEMSET = $(MANPAGES_1_MD_PMEMSET:.1.md=.1)

ifeq ($(EXPERIMENTAL),y)
MANPAGES_7_GROFF += $(MANPAGES_7_GROFF_EXP)
MANPAGES_5_GROFF += $(MANPAGES_5_GROFF_EXP)
MANPAGES_3_GROFF += $(MANPAGES_3_GROFF_EXP)
MANPAGES_1_GROFF += $(MANPAGES_1_GROFF_EXP)
else
MANPAGES_7_NOINSTALL += $(MANPAGES_7_GROFF_EXP)
MANPAGES_5_NOINSTALL += $(MANPAGES_5_GROFF_EXP)
MANPAGES_3_NOINSTALL += $(MANPAGES_3_GROFF_EXP)
MANPAGES_1_NOINSTALL += $(MANPAGES_1_GROFF_EXP)
endif

MANPAGES_7_GROFF += $(MANPAGES_7_GROFF_PMEM2)
MANPAGES_5_GROFF += $(MANPAGES_5_GROFF_PMEM2)
MANPAGES_3_GROFF += $(MANPAGES_3_GROFF_PMEM2)
MANPAGES_1_GROFF += $(MANPAGES_1_GROFF_PMEM2)

ifeq ($(PMEMSET_INSTALL),y)
MANPAGES_7_GROFF += $(MANPAGES_7_GROFF_PMEMSET)
MANPAGES_5_GROFF += $(MANPAGES_5_GROFF_PMEMSET)
MANPAGES_3_GROFF += $(MANPAGES_3_GROFF_PMEMSET)
MANPAGES_1_GROFF += $(MANPAGES_1_GROFF_PMEMSET)
else
MANPAGES_7_NOINSTALL += $(MANPAGES_7_GROFF_PMEMSET)
MANPAGES_5_NOINSTALL += $(MANPAGES_5_GROFF_PMEMSET)
MANPAGES_3_NOINSTALL += $(MANPAGES_3_GROFF_PMEMSET)
MANPAGES_1_NOINSTALL += $(MANPAGES_1_GROFF_PMEMSET)
endif

MANPAGES_7_MD_WEB = $(MANPAGES_7_MD_PMEM2) $(MANPAGES_7_MD_PMEMSET) $(MANPAGES_7_MD)
MANPAGES_5_MD_WEB = $(MANPAGES_5_MD_PMEM2) $(MANPAGES_5_MD_PMEMSET) $(MANPAGES_5_MD)
MANPAGES_3_MD_WEB = $(MANPAGES_3_MD_PMEM2) $(MANPAGES_3_MD_PMEMSET) $(MANPAGES_3_MD)
MANPAGES_1_MD_WEB = $(MANPAGES_1_MD_PMEM2) $(MANPAGES_1_MD_PMEMSET) $(MANPAGES_1_MD)

MANPAGES = $(MANPAGES_7_GROFF) $(MANPAGES_5_GROFF) $(MANPAGES_3_GROFF) $(MANPAGES_1_GROFF) \
	   $(MANPAGES_7_NOINSTALL) $(MANPAGES_5_NOINSTALL) $(MANPAGES_3_NOINSTALL) $(MANPAGES_1_NOINSTALL)

HTMLFILES = $(MANPAGES:=.html)
TXTFILES = $(MANPAGES:=.txt)

GZFILES_7 = $(MANPAGES_7_GROFF:=.gz)
GZFILES_5 = $(MANPAGES_5_GROFF:=.gz)
GZFILES_3 = $(MANPAGES_3_GROFF:=.gz)
GZFILES_1 = $(MANPAGES_1_GROFF:=.gz)
GZFILES_7_NOINSTALL = $(MANPAGES_7_NOINSTALL:=.gz)
GZFILES_5_NOINSTALL = $(MANPAGES_5_NOINSTALL:=.gz)
GZFILES_3_NOINSTALL = $(MANPAGES_3_NOINSTALL:=.gz)
GZFILES_1_NOINSTALL = $(MANPAGES_1_NOINSTALL:=.gz)
GZFILES_3_DUMMY = $(MANPAGES_3_DUMMY:=.gz)

GZFILES = $(GZFILES_7) $(GZFILES_5) $(GZFILES_3) $(GZFILES_1) \
	  $(GZFILES_7_NOINSTALL) $(GZFILES_5_NOINSTALL) $(GZFILES_3_NOINSTALL) $(GZFILES_1_NOINSTALL) \
	  $(GZFILES_3_DUMMY)

MANPAGES_DESTDIR_7 = $(DESTDIR)$(man7dir)
MANPAGES_DESTDIR_5 = $(DESTDIR)$(man5dir)
MANPAGES_DESTDIR_3 = $(DESTDIR)$(man3dir)
MANPAGES_DESTDIR_1 = $(DESTDIR)$(man1dir)

DOCS_DESTDIR = $(DESTDIR)$(docdir)

txt: $(TXTFILES)

all: | $(MANPAGES)

$(MANPAGES_WEBDIR_LINUX) $(MANPAGES_WEBDIR_WINDOWS):
	$(MKDIR) -p $@

%.1: %.1.md
	../utils/md2man.sh $< default.man $@

%.3: %.3.md
	../utils/md2man.sh $< default.man $@

%.5: %.5.md
	../utils/md2man.sh $< default.man $@

%.7: %.7.md
	../utils/md2man.sh $< default.man $@

%.txt: %
	man ./$< > $@

%.gz: %
	gzip -nc ./$* > $@

%.html: %
	groff -mandoc -Thtml ./$< > $@

html: $(HTMLFILES)

web: $(MANPAGES) | $(MANPAGES_WEBDIR_LINUX) $(MANPAGES_WEBDIR_WINDOWS)
	$(MAKE) -C generated all
	$(foreach f, $(MANPAGES_7_MD_WEB), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_5_MD_WEB), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_3_MD_WEB), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_1_MD_WEB), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_7_MD_WEB), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_5_MD_WEB), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_3_MD_WEB), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_1_MD_WEB), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)

compress: $(GZFILES)

clean:

clobber: clean
	$(RM) $(TXTFILES) $(HTMLFILES) $(GZFILES) $(MANPAGES)
	$(RM) -r $(MANPAGES_WEBDIR_LINUX) \
		$(MANPAGES_WEBDIR_WINDOWS)

install: compress
	install -d -v $(MANPAGES_DESTDIR_7)
	install -p -m 0644 $(GZFILES_7) $(MANPAGES_DESTDIR_7)
	install -d -v $(MANPAGES_DESTDIR_5)
	install -p -m 0644 $(GZFILES_5) $(MANPAGES_DESTDIR_5)
	install -d -v $(MANPAGES_DESTDIR_3)
	install -p -m 0644 $(GZFILES_3) $(GZFILES_3_DUMMY) $(MANPAGES_DESTDIR_3)
	install -d -v $(MANPAGES_DESTDIR_1)
	install -p -m 0644 $(GZFILES_1) $(MANPAGES_DESTDIR_1)

uninstall:
	$(foreach f, $(notdir $(GZFILES_7)), $(RM) $(MANPAGES_DESTDIR_7)/$(f))
	$(foreach f, $(notdir $(GZFILES_5)), $(RM) $(MANPAGES_DESTDIR_5)/$(f))
	$(foreach f, $(notdir $(GZFILES_3) $(GZFILES_3_DUMMY)), $(RM) $(MANPAGES_DESTDIR_3)/$(f))
	$(foreach f, $(notdir $(GZFILES_1)), $(RM) $(MANPAGES_DESTDIR_1)/$(f))

FORCE:

.PHONY: all html clean compress clobber cstyle install uninstall
