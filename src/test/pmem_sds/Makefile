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
# src/test/pmem_sds/Makefile -- build pmem_sds unittest
#
TARGET = pmem_sds
OBJS = pmem_sds.o ras_mock.o

vpath %.c $(TOP)/src/libpmem
vpath %.c $(TOP)/src/common

BUILD_STATIC=n

OBJS +=\
	$(TOP)/src/debug/common/file.o\
	$(TOP)/src/debug/common/file_linux.o\
	$(TOP)/src/debug/common/os_linux.o\
	$(TOP)/src/debug/common/os_thread_linux.o\
	$(TOP)/src/debug/common/out.o\
	$(TOP)/src/debug/common/util.o\
	$(TOP)/src/debug/common/util_linux.o\
	$(TOP)/src/debug/common/mmap.o\
	$(TOP)/src/debug/common/mmap_linux.o\
	$(TOP)/src/debug/libpmem/libpmem.o\
	$(TOP)/src/debug/libpmem/cpu.o\
	$(TOP)/src/debug/libpmem/pmem.o\
	$(TOP)/src/debug/libpmem/pmem_linux.o

include ../Makefile.inc

