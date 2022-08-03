# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation
#
# Makefile -- top Makefile for pmempool
#

vpath %.c ../../libpmemobj/

TARGET = pmempool

OBJS = pmempool.o\
       info.o info_blk.o info_log.o info_obj.o ulog.o\
       create.o dump.o check.o rm.o convert.o synchronize.o transform.o feature.o\

LIBPMEM=y
LIBPMEMBLK=y
LIBPMEMOBJ=y
LIBPMEMLOG=y
LIBPMEMPOOL=y
TOOLS_COMMON=y

LIBPMEMOBJ_PRIV=memblock_from_offset alloc_class_by_id\
	memblock_rebuild_state alloc_class_by_run\
	heap_run_foreach_object alloc_class_collection_new\
	alloc_class_collection_delete

LIBPMEMBLK_PRIV=btt_init btt_write btt_fini btt_info_convert2h\
	btt_info_convert2le btt_flog_convert2h btt_flog_convert2le

INCS += -I$(TOP)/src/common
INCS += -I$(TOP)/src/libpmem2
INCS += -I$(TOP)/src/libpmemlog
INCS += -I$(TOP)/src/libpmemblk
INCS += -I$(TOP)/src/libpmemobj

MANPAGES = $(TOP)/doc/pmempool.1\
           $(TOP)/doc/pmempool-info.1\
	   $(TOP)/doc/pmempool-create.1\
	   $(TOP)/doc/pmempool-check.1\
	   $(TOP)/doc/pmempool-dump.1\
	   $(TOP)/doc/pmempool-rm.1\
	   $(TOP)/doc/pmempool-convert.1\
	   $(TOP)/doc/pmempool-sync.1\
	   $(TOP)/doc/pmempool-transform.1

BASH_COMP_FILES = bash_completion/pmempool

include ../Makefile.inc

.PHONY: test check
