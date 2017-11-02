# Copyright 2014-2018, Intel Corporation
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
# src/libpmem/Makefile -- Makefile for libpmem
#

LIBRARY_NAME = pmem
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0
SOURCE =\
	$(COMMON)/file.c\
	$(COMMON)/file_posix.c\
	$(COMMON)/mmap.c\
	$(COMMON)/mmap_posix.c\
	$(COMMON)/os_posix.c\
	$(COMMON)/os_thread_posix.c\
	$(COMMON)/os_deep_linux.c\
	$(COMMON)/out.c\
	$(COMMON)/util.c\
	$(COMMON)/util_posix.c\
	libpmem.c\
	cpu.c\
	pmem.c\
	pmem_posix.c \
	memcpy_nt_avx.c\
	memcpy_nt_sse2.c\
	memset_nt_avx.c\
	memset_nt_sse2.c\
	memcpy_t_avx.c\
	memcpy_t_sse2.c\
	memset_t_avx.c\
	memset_t_sse2.c

AVX512F_PROG="\#include <immintrin.h>\n\#include <stdint.h>\nint main(){ uint64_t v[8]; __m512i zmm0 = _mm512_loadu_si512((__m512i *)&v); return 0;}"
AVX512F_AVAILABLE := $(shell printf $(AVX512F_PROG) |\
	$(CC) $(CFLAGS) -x c -mavx512f -o /dev/null - 2>/dev/null && echo y || echo n)

ifeq ($(AVX512F_AVAILABLE), y)
SOURCE += memcpy_nt_avx512f.c\
	memset_nt_avx512f.c\
	memcpy_t_avx512f.c\
	memset_t_avx512f.c
endif

include ../Makefile.inc

vpath %.c $(TOP)/src/libpmem/memcpy
vpath %.c $(TOP)/src/libpmem/memset

$(objdir)/memcpy_nt_avx512f.o: CFLAGS += -mavx512f
$(objdir)/memset_nt_avx512f.o: CFLAGS += -mavx512f
$(objdir)/memcpy_nt_avx.o: CFLAGS += -mavx
$(objdir)/memset_nt_avx.o: CFLAGS += -mavx

$(objdir)/memcpy_t_avx512f.o: CFLAGS += -mavx512f
$(objdir)/memset_t_avx512f.o: CFLAGS += -mavx512f
$(objdir)/memcpy_t_avx.o: CFLAGS += -mavx
$(objdir)/memset_t_avx.o: CFLAGS += -mavx

CFLAGS += -I.
ifeq ($(AVX512F_AVAILABLE), y)
CFLAGS += -DAVX512F_AVAILABLE=1
else
CFLAGS += -DAVX512F_AVAILABLE=0
endif

LIBS += -pthread
