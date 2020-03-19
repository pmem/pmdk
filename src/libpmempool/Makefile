# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

#
# src/libpmempool/Makefile -- Makefile for libpmempool
#

LIBRARY_NAME = pmempool
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0

INCS += -I$(TOP)/src/libpmemlog
INCS += -I$(TOP)/src/libpmemblk
INCS += -I$(TOP)/src/libpmemobj
INCS += -I$(TOP)/src/rpmem_common
INCS += -I$(TOP)/src/librpmem
INCS += -I$(TOP)/src/libpmem2

vpath %.c ../librpmem

include ../core/pmemcore.inc
include ../common/pmemcommon.inc

SOURCE +=\
	libpmempool.c\
	check.c\
	check_bad_blocks.c\
	check_backup.c\
	check_btt_info.c\
	check_btt_map_flog.c\
	check_log.c\
	check_blk.c\
	check_pool_hdr.c\
	check_sds.c\
	check_util.c\
	check_write.c\
	pool.c\
	replica.c\
	feature.c\
	$(RPMEM_COMMON)/rpmem_common.c\
	rpmem_ssh.c\
	rpmem_cmd.c\
	rpmem_util.c\
	sync.c\
	transform.c\
	rm.c

LIBPMEMBLK_PRIV_FUNCS=btt_info_set btt_arena_datasize btt_flog_size\
	btt_map_size btt_flog_get_valid map_entry_is_initial btt_info_convert2h\
	btt_info_convert2le btt_flog_convert2h btt_flog_convert2le

include ../Makefile.inc

CFLAGS += $(LIBNDCTL_CFLAGS)
LIBS += -pthread -lpmem $(LIBDL) $(LIBNDCTL_LIBS)
CFLAGS += -DUSE_LIBDL
CFLAGS += -DUSE_RPMEM

pmemblk_priv_funcs.o: $(PMEMBLK_PRIV_OBJ)
	$(OBJCOPY) --localize-hidden $(addprefix -G, $(LIBPMEMBLK_PRIV_FUNCS)) \
	$< $@
