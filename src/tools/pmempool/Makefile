# Copyright 2014-2020, Intel Corporation
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
# Makefile -- top Makefile for pmempool
#
SCP_TO_REMOTE_NODES = y

vpath %.c ../../libpmemobj/
vpath %.c ../../librpmem/
vpath %.c ../../rpmem_common/

TARGET = pmempool

OBJS = pmempool.o\
       info.o info_blk.o info_log.o info_obj.o ulog.o\
       create.o dump.o check.o rm.o convert.o synchronize.o transform.o\
       rpmem_ssh.o rpmem_cmd.o rpmem_util.o rpmem_common.o feature.o

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
INCS += -I$(TOP)/src/rpmem_common
INCS += -I$(TOP)/src/librpmem
INCS += -I$(TOP)/src/libpmemlog
INCS += -I$(TOP)/src/libpmemblk
INCS += -I$(TOP)/src/libpmemobj

CFLAGS += -DUSE_RPMEM

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
