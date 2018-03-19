#
# Copyright 2017-2018, Intel Corporation
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
# src/test/pmem_deep_persist/Makefile -- build pmem_deep_persist test
#
include ../../common.inc
ARCH := $(call get_arch)

TOP = ../../..
vpath %.c $(TOP)/src/common
vpath %.c $(TOP)/src/libpmem
vpath %.c $(TOP)/src/libpmem/$(ARCH)

TARGET = pmem_deep_persist

OBJS = pmem_deep_persist.o\
	mmap.o\
	mmap_posix.o\
	util.o\
	util_posix.o\
	file.o\
	file_posix.o\
	os_posix.o\
	out.o\
	fs_posix.o\
	libpmem.o\
	pmem.o\
	pmem_posix.o\
	uuid.o\
	$(call osdep, uuid,.o)\
	pool_hdr.o\
	set.o\
	os_deep_linux.o\
	os_auto_flush_linux.o\
	shutdown_state.o\
	os_dimm_none.o\
	init.o\
	$(call osdep, extent,.o)\
	$(call osdep, badblock,.o)\
	badblock_poolset.o
# this test does not require ndctl
# to enable builds we use os_dimm_none

ifeq ($(ARCH), x86_64)
OBJS += cpu.o
endif

LIBPMEMOBJ=y
LIBPMEM=y

include ../Makefile.inc

CFLAGS += -DSRCVERSION="" -DDEBUG -DAVX512F_AVAILABLE=0 -DAVX_AVAILABLE=0 -DSSE2_AVAILABLE=0
CFLAGS += -I$(TOP)/src/libpmem
CFLAGS += -I$(TOP)/src/common
CFLAGS += -I$(TOP)/src/libpmemobj/
LDFLAGS += $(call extract_funcs, pmem_deep_persist.c)
