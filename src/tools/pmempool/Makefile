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
# Makefile -- top Makefile for pmempool
#

vpath %.c ../../
vpath %.c ../../common
vpath %.c ../../libpmemblk


TARGET = pmempool

OBJS = pmempool.o info.o info_blk.o info_log.o info_obj.o create.o dump.o\
       common.o output.o util.o check.o btt.o set.o

LIBPMEM=y
LIBPMEMBLK=y
LIBPMEMOBJ=y
LIBPMEMLOG=y

INCS += -I../../common
INCS += -I../../libpmemlog
INCS += -I../../libpmemblk
INCS += -I../../libpmemobj

MANPAGES = ../../../doc/pmempool.1\
           ../../../doc/pmempool-info.1\
	   ../../../doc/pmempool-create.1\
	   ../../../doc/pmempool-check.1\
	   ../../../doc/pmempool-dump.1

BASH_COMP_FILES = pmempool.sh

include ../Makefile.inc

.PHONY: test check
