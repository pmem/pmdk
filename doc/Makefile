#
# Copyright 2014-2019, Intel Corporation
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
# doc/Makefile -- Makefile for PMDK man pages
#

include ../src/common.inc

MANPAGES_7_MD = libpmem/libpmem.7.md libpmemblk/libpmemblk.7.md libpmemlog/libpmemlog.7.md \
		libpmemobj/libpmemobj.7.md libpmempool/libpmempool.7.md libvmem/libvmem.7.md \
		libvmmalloc/libvmmalloc.7.md libpmem2/libpmem2.7.md

MANPAGES_5_MD = poolset/poolset.5.md pmem_ctl/pmem_ctl.5.md

MANPAGES_3_MD = libpmem/pmem_flush.3.md libpmem/pmem_is_pmem.3.md libpmem/pmem_memmove_persist.3.md \
		libpmemblk/pmemblk_bsize.3.md libpmemblk/pmemblk_create.3.md libpmemblk/pmemblk_ctl_get.3.md libpmemblk/pmemblk_read.3.md libpmemblk/pmemblk_set_zero.3.md \
		libpmemlog/pmemlog_append.3.md libpmemlog/pmemlog_create.3.md libpmemlog/pmemlog_ctl_get.3.md libpmemlog/pmemlog_nbyte.3.md libpmemlog/pmemlog_tell.3.md \
		libpmemobj/oid_is_null.3.md libpmemobj/pmemobj_action.3.md libpmemobj/pmemobj_alloc.3.md libpmemobj/pmemobj_ctl_get.3.md libpmemobj/pmemobj_first.3.md \
		libpmemobj/pmemobj_list_insert.3.md libpmemobj/pmemobj_memcpy_persist.3.md libpmemobj/pmemobj_mutex_zero.3.md \
		libpmemobj/pmemobj_open.3.md libpmemobj/pmemobj_root.3.md libpmemobj/pmemobj_tx_begin.3.md libpmemobj/pmemobj_tx_add_range.3.md \
		libpmemobj/pmemobj_tx_alloc.3.md libpmemobj/pobj_layout_begin.3.md libpmemobj/pobj_list_head.3.md libpmemobj/toid_declare.3.md \
		libpmempool/pmempool_check_init.3.md libpmempool/pmempool_feature_query.3.md libpmempool/pmempool_rm.3.md libpmempool/pmempool_sync.3.md \
		libvmem/vmem_create.3.md libvmem/vmem_malloc.3.md \
		libpmem2/pmem2_errormsg.3.md


MANPAGES_1_MD = pmempool/pmempool.1.md pmempool/pmempool-info.1.md pmempool/pmempool-create.1.md \
		pmempool/pmempool-check.1.md pmempool/pmempool-dump.1.md pmempool/pmempool-rm.1.md \
		pmempool/pmempool-convert.1.md pmempool/pmempool-sync.1.md pmempool/pmempool-transform.1.md \
		pmempool/pmempool-feature.1.md pmreorder/pmreorder.1.md


MANPAGES_3_DUMMY = pmem_drain.3 pmem_has_hw_drain.3 pmem_has_auto_flush.3 \
		   pmem_persist.3 pmem_msync.3 pmem_map_file.3 pmem_deep_persist.3 pmem_deep_flush.3 pmem_deep_drain.3 pmem_unmap.3 \
		   pmem_memcpy_persist.3 pmem_memset_persist.3 pmem_memmove_nodrain.3 pmem_memcpy_nodrain.3 pmem_memset_nodrain.3 \
		   pmem_memcpy.3 pmem_memset.3 pmem_memmove.3 \
		   pmem_check_version.3 pmem_errormsg.3 \
		   pmemblk_nblock.3 \
		   pmemblk_open.3 pmemblk_close.3 \
		   pmemblk_write.3 \
		   pmemblk_set_error.3 \
		   pmemblk_check_version.3 pmemblk_check.3 pmemblk_errormsg.3 pmemblk_set_funcs.3 \
		   pmemblk_ctl_set.3 pmemblk_ctl_exec.3\
		   pmemlog_rewind.3 pmemlog_walk.3 \
		   pmemlog_open.3 pmemlog_close.3 \
		   pmemlog_appendv.3 \
		   pmemlog_check_version.3 pmemlog_check.3 pmemlog_errormsg.3 pmemlog_set_funcs.3 \
		   pmemlog_ctl_set.3 pmemlog_ctl_exec.3\
		   pmempool_check.3 pmempool_check_end.3 \
		   pmempool_feature_enable.3 pmempool_feature_disable.3 \
		   pmempool_transform.3 \
		   pmempool_check_version.3 pmempool_errormsg.3 \
		   vmem_create_in_region.3 vmem_delete.3 vmem_check.3 vmem_stats_print.3 \
		   vmem_calloc.3 vmem_realloc.3 vmem_free.3 vmem_aligned_alloc.3 vmem_strdup.3 vmem_wcsdup.3 vmem_malloc_usable_size.3 \
		   vmem_check_version.3 vmem_errormsg.3 vmem_set_funcs.3 \
		   oid_equals.3 pmemobj_direct.3 pmemobj_oid.3 pmemobj_type_num.3 pmemobj_pool_by_oid.3 pmemobj_pool_by_ptr.3 pmemobj_volatile.3\
		   pmemobj_zalloc.3 pmemobj_xalloc.3 pmemobj_free.3 pmemobj_realloc.3 pmemobj_zrealloc.3 pmemobj_strdup.3 pmemobj_wcsdup.3 pmemobj_alloc_usable_size.3 \
		   pobj_new.3 pobj_alloc.3 pobj_znew.3 pobj_zalloc.3 pobj_realloc.3 pobj_zrealloc.3 pobj_free.3 \
		   pobj_layout_toid.3 pobj_layout_root.3 pobj_layout_name.3 pobj_layout_end.3 pobj_layout_types_num.3 \
		   pmemobj_ctl_set.3 pmemobj_ctl_exec.3\
		   pmemobj_create.3 pmemobj_close.3 \
		   pmemobj_list_insert_new.3 pmemobj_list_remove.3 pmemobj_list_move.3 \
		   toid_declare_root.3 toid.3 toid_type_num.3 toid_type_num_of.3 toid_valid.3 oid_instanceof.3 toid_assign.3 toid_is_null.3 toid_equals.3 toid_typeof.3 toid_offsetof.3 direct_rw.3 d_rw.3 direct_ro.3 d_ro.3 \
		   pmemobj_memcpy.3 pmemobj_memmove.3 pmemobj_memset.3 \
		   pmemobj_memset_persist.3 pmemobj_persist.3 pmemobj_xpersist.3 pmemobj_flush.3 pmemobj_xflush.3 pmemobj_drain.3 \
		   pmemobj_tx_stage.3 pmemobj_tx_lock.3 pmemobj_tx_xlock.3 pmemobj_tx_abort.3 pmemobj_tx_commit.3 pmemobj_tx_end.3 pmemobj_tx_errno.3 \
		   pmemobj_tx_process.3 pmemobj_tx_add_range_direct.3 pmemobj_tx_xadd_range.3 pmemobj_tx_xadd_range_direct.3 \
		   pmemobj_tx_zalloc.3 pmemobj_tx_xalloc.3 pmemobj_tx_realloc.3 pmemobj_tx_zrealloc.3 pmemobj_tx_strdup.3 pmemobj_tx_xstrdup.3 pmemobj_tx_wcsdup.3 pmemobj_tx_xwcsdup.3 pmemobj_tx_free.3 pmemobj_tx_xfree.3\
		   pmemobj_tx_log_append_buffer.3 pmemobj_tx_xlog_append_buffer.3 pmemobj_tx_log_auto_alloc.3 pmemobj_tx_log_snapshots_max_size.3 pmemobj_tx_log_intents_max_size.3 \
		   tx_begin_param.3 tx_begin_cb.3 tx_begin.3 tx_onabort.3 tx_oncommit.3 tx_finally.3 tx_end.3 \
		   tx_add.3 tx_add_field.3 tx_add_direct.3 tx_add_field_direct.3 tx_xadd.3 tx_xadd_field.3 tx_xadd_direct.3 tx_xadd_field_direct.3 \
		   tx_new.3 tx_alloc.3 tx_znew.3 tx_zalloc.3 tx_xalloc.3 tx_realloc.3 tx_zrealloc.3 tx_strdup.3 tx_wcsdup.3 tx_free.3 tx_set.3 tx_set_direct.3 tx_memcpy.3 tx_memset.3 \
		   pmemobj_f_mem_nodrain.3 pmemobj_f_mem_nontemporal.3 pmemobj_f_mem_temporal.3 pmemobj_f_mem_wc.3 pmemobj_f_mem_wb.3 pmemobj_f_mem_noflush.3 pmemobj_f_relaxed.3 \
		   pmemobj_mutex_lock.3 pmemobj_mutex_timedlock.3 pmemobj_mutex_trylock.3 pmemobj_mutex_unlock.3 \
		   pmemobj_rwlock_zero.3 pmemobj_rwlock_rdlock.3 pmemobj_rwlock_wrlock.3 pmemobj_rwlock_timedrdlock.3 pmemobj_rwlock_timedwrlock.3 pmemobj_rwlock_tryrdlock.3 pmemobj_rwlock_trywrlock.3 pmemobj_rwlock_unlock.3 \
		   pmemobj_cond_zero.3 pmemobj_cond_broadcast.3 pmemobj_cond_signal.3 pmemobj_cond_timedwait.3 pmemobj_cond_wait.3 \
		   pobj_list_entry.3 pobj_list_first.3 pobj_list_last.3 pobj_list_empty.3 pobj_list_next.3 pobj_list_prev.3 pobj_list_foreach.3 pobj_list_foreach_reverse.3 \
		   pobj_list_insert_head.3 pobj_list_insert_tail.3 pobj_list_insert_after.3 pobj_list_insert_before.3 pobj_list_insert_new_head.3 pobj_list_insert_new_tail.3 \
		   pobj_list_insert_new_after.3 pobj_list_insert_new_before.3 pobj_list_remove.3 pobj_list_remove_free.3 \
		   pobj_list_move_element_head.3 pobj_list_move_element_tail.3 pobj_list_move_element_after.3 pobj_list_move_element_before.3 \
		   pmemobj_next.3 pobj_first_type_num.3 pobj_first.3 pobj_next_type_num.3 pobj_next.3 pobj_foreach.3 pobj_foreach_safe.3 pobj_foreach_type.3 pobj_foreach_safe_type.3 \
		   pmemobj_root_construct.3 pobj_root.3 pmemobj_root_size.3 \
		   pmemobj_check_version.3 pmemobj_check.3 pmemobj_errormsg.3 pmemobj_set_funcs.3 \
		   pmemobj_reserve.3 pmemobj_xreserve.3 pmemobj_defer_free.3 pmemobj_set_value.3 pmemobj_publish.3 pmemobj_tx_publish.3 pmemobj_tx_xpublish.3 pmemobj_cancel.3 pobj_reserve_new.3 pobj_reserve_alloc.3 pobj_xreserve_new.3 pobj_xreserve_alloc.3


MANPAGES_BUILDDIR = generated
MANPAGES_WEBDIR_LINUX = web_linux
MANPAGES_WEBDIR_WINDOWS = web_windows


# experimental
MANPAGES_3_MD_EXP =
MANPAGES_1_MD_EXP =

MANPAGES_3_MD_DUMMY_EXP =
MANPAGES_1_MD_DUMMY_EXP =

ifeq ($(BUILD_RPMEM),y)
MANPAGES_7_MD += librpmem/librpmem.7.md
MANPAGES_3_MD += librpmem/rpmem_create.3.md
MANPAGES_3_MD += librpmem/rpmem_persist.3.md
MANPAGES_1_MD += rpmemd/rpmemd.1.md
MANPAGES_3_DUMMY += rpmem_open.3 rpmem_set_attr.3 rpmem_close.3 \
		    rpmem_read.3 rpmem_remove.3 rpmem_check_version.3 \
		    rpmem_errormsg.3 rpmem_deep_persist.3 rpmem_flush.3 \
		    rpmem_drain.3
endif

ifeq ($(NDCTL_ENABLE),y)
MANPAGES_1_MD += daxio/daxio.1.md
endif

MANPAGES_7_GROFF = $(MANPAGES_7_MD:.7.md=.7)
MANPAGES_5_GROFF = $(MANPAGES_5_MD:.5.md=.5)
MANPAGES_3_GROFF = $(MANPAGES_3_MD:.3.md=.3)
MANPAGES_1_GROFF = $(MANPAGES_1_MD:.1.md=.1)

MANPAGES_7_GROFF_EXP = $(MANPAGES_7_MD_EXP:.3.md=.7)
MANPAGES_5_GROFF_EXP = $(MANPAGES_5_MD_EXP:.3.md=.5)
MANPAGES_3_GROFF_EXP = $(MANPAGES_3_MD_EXP:.3.md=.3)
MANPAGES_1_GROFF_EXP = $(MANPAGES_1_MD_EXP:.1.md=.1)

ifeq ($(EXPERIMENTAL),y)
$(MANPAGES_7_GROFF) += $(MANPAGES_7_GROFF_EXP)
$(MANPAGES_5_GROFF) += $(MANPAGES_5_GROFF_EXP)
$(MANPAGES_3_GROFF) += $(MANPAGES_3_GROFF_EXP)
$(MANPAGES_1_GROFF) += $(MANPAGES_1_GROFF_EXP)
else
MANPAGES_7_NOINSTALL += $(MANPAGES_7_GROFF_EXP)
MANPAGES_5_NOINSTALL += $(MANPAGES_5_GROFF_EXP)
MANPAGES_3_NOINSTALL += $(MANPAGES_3_GROFF_EXP)
MANPAGES_1_NOINSTALL += $(MANPAGES_1_GROFF_EXP)
endif

MANPAGES = $(MANPAGES_7_GROFF) $(MANPAGES_5_GROFF) $(MANPAGES_3_GROFF) $(MANPAGES_1_GROFF) \
	   $(MANPAGES_7_NOINSTALL) $(MANPAGES_5_NOINSTALL) $(MANPAGES_3_NOINSTALL) $(MANPAGES_1_NOINSTALL)

MANPAGES_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(MANPAGES)))

HTMLFILES = $(MANPAGES_BUILD:=.html)
TXTFILES = $(MANPAGES_BUILD:=.txt)

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

GZFILES_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES)))
GZFILES_7_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_7)))
GZFILES_5_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_5)))
GZFILES_3_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_3)))
GZFILES_1_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_1)))

GZFILES_3_BUILD += $(addprefix $(MANPAGES_BUILDDIR)/, $(GZFILES_3_DUMMY))

MANPAGES_DESTDIR_7 = $(DESTDIR)$(man7dir)
MANPAGES_DESTDIR_5 = $(DESTDIR)$(man5dir)
MANPAGES_DESTDIR_3 = $(DESTDIR)$(man3dir)
MANPAGES_DESTDIR_1 = $(DESTDIR)$(man1dir)

DOCS_DESTDIR = $(DESTDIR)$(docdir)

all: md2man $(TXTFILES) | $(MANPAGES_BUILDDIR)

$(MANPAGES_BUILDDIR) $(MANPAGES_WEBDIR_LINUX) $(MANPAGES_WEBDIR_WINDOWS):
	$(MKDIR) -p $@

%.txt: %
	man ./$< > $@

groff: $(MANPAGES_7_GROFF) $(MANPAGES_5_GROFF) $(MANPAGES_3_GROFF) $(MANPAGES_1_GROFF)

html: $(HTMLFILES)

%.html: %
	groff -mandoc -Thtml ./$< > $@

md2man:
	$(foreach f, $(MANPAGES_7_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_5_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_3_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_1_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)

web: | $(MANPAGES_WEBDIR_LINUX) $(MANPAGES_WEBDIR_WINDOWS)
	$(MAKE) -C generated all
	$(foreach f, $(MANPAGES_7_MD), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_5_MD), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_3_MD), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_1_MD), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_7_MD), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_5_MD), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_3_MD), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_1_MD), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)

compress: $(GZFILES_BUILD)

%.gz:
	gzip -c ./$* > $@

clean:

clobber: clean
	$(RM) -rf $(MANPAGES_BUILDDIR)/*.txt \
		$(MANPAGES_BUILDDIR)/*.html \
		$(MANPAGES_BUILDDIR)/*.gz \
		$(MANPAGES_WEBDIR_LINUX) \
		$(MANPAGES_WEBDIR_WINDOWS)

install: compress
	install -d -v $(MANPAGES_DESTDIR_7)
	install -p -m 0644 $(GZFILES_7_BUILD) $(MANPAGES_DESTDIR_7)
	install -d -v $(MANPAGES_DESTDIR_5)
	install -p -m 0644 $(GZFILES_5_BUILD) $(MANPAGES_DESTDIR_5)
	install -d -v $(MANPAGES_DESTDIR_3)
	install -p -m 0644 $(GZFILES_3_BUILD) $(MANPAGES_DESTDIR_3)
	install -d -v $(MANPAGES_DESTDIR_1)
	install -p -m 0644 $(GZFILES_1_BUILD) $(MANPAGES_DESTDIR_1)

uninstall:
	$(foreach f, $(notdir $(GZFILES_7_BUILD)), $(RM) $(MANPAGES_DESTDIR_7)/$(f))
	$(foreach f, $(notdir $(GZFILES_5_BUILD)), $(RM) $(MANPAGES_DESTDIR_5)/$(f))
	$(foreach f, $(notdir $(GZFILES_3_BUILD)), $(RM) $(MANPAGES_DESTDIR_3)/$(f))
	$(foreach f, $(notdir $(GZFILES_1_BUILD)), $(RM) $(MANPAGES_DESTDIR_1)/$(f))

FORCE:

.PHONY: all html clean compress clobber cstyle install uninstall
