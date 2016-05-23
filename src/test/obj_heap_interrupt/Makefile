#
# Copyright 2016, Intel Corporation
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
# src/test/obj_heap_interrupt/Makefile -- build obj_heap_interrupt unit test
#
vpath %.c ../../libpmemobj
vpath %.c ../../common

TARGET = obj_heap_interrupt
OBJS = obj_heap_interrupt.o pmalloc.o bucket.o redo.o heap.o lane.o ctree.o\
    util.o util_linux.o set.o set_linux.o out.o obj.o cuckoo.o\
    list.o sync.o tx.o memops.o memblock.o libpmemobj.o pvector.o

LIBPMEM=y
LIBPMEMOBJ=y

out.o: CFLAGS += -DSRCVERSION=\"utversion\"

EXTRA_CFLAGS += -DDEBUG

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, obj_heap_interrupt.c)
INCS += -I../../libpmemobj/ -I../../common/
