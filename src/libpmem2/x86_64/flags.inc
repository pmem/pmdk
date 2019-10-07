# Copyright 2018, Intel Corporation
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
# src/libpmem/x86_64/flags.inc -- flags for libpmem/x86_64
#

vpath %.c $(TOP)/src/libpmem/x86_64
vpath %.h $(TOP)/src/libpmem/x86_64
vpath %.c $(TOP)/src/libpmem/x86_64/memcpy
vpath %.c $(TOP)/src/libpmem/x86_64/memset

$(objdir)/memcpy_nt_avx512f_clflush.o: CFLAGS += -mavx512f
$(objdir)/memcpy_nt_avx512f_clflushopt.o: CFLAGS += -mavx512f
$(objdir)/memcpy_nt_avx512f_clwb.o: CFLAGS += -mavx512f
$(objdir)/memcpy_nt_avx512f_empty.o: CFLAGS += -mavx512f
$(objdir)/memset_nt_avx512f_clflush.o: CFLAGS += -mavx512f
$(objdir)/memset_nt_avx512f_clflushopt.o: CFLAGS += -mavx512f
$(objdir)/memset_nt_avx512f_clwb.o: CFLAGS += -mavx512f
$(objdir)/memset_nt_avx512f_empty.o: CFLAGS += -mavx512f
$(objdir)/memcpy_nt_avx_clflush.o: CFLAGS += -mavx
$(objdir)/memcpy_nt_avx_clflushopt.o: CFLAGS += -mavx
$(objdir)/memcpy_nt_avx_clwb.o: CFLAGS += -mavx
$(objdir)/memcpy_nt_avx_empty.o: CFLAGS += -mavx
$(objdir)/memset_nt_avx_clflush.o: CFLAGS += -mavx
$(objdir)/memset_nt_avx_clflushopt.o: CFLAGS += -mavx
$(objdir)/memset_nt_avx_clwb.o: CFLAGS += -mavx
$(objdir)/memset_nt_avx_empty.o: CFLAGS += -mavx

$(objdir)/memcpy_t_avx512f_clflush.o: CFLAGS += -mavx512f
$(objdir)/memcpy_t_avx512f_clflushopt.o: CFLAGS += -mavx512f
$(objdir)/memcpy_t_avx512f_clwb.o: CFLAGS += -mavx512f
$(objdir)/memcpy_t_avx512f_empty.o: CFLAGS += -mavx512f
$(objdir)/memset_t_avx512f_clflush.o: CFLAGS += -mavx512f
$(objdir)/memset_t_avx512f_clflushopt.o: CFLAGS += -mavx512f
$(objdir)/memset_t_avx512f_clwb.o: CFLAGS += -mavx512f
$(objdir)/memset_t_avx512f_empty.o: CFLAGS += -mavx512f
$(objdir)/memcpy_t_avx_clflush.o: CFLAGS += -mavx
$(objdir)/memcpy_t_avx_clflushopt.o: CFLAGS += -mavx
$(objdir)/memcpy_t_avx_clwb.o: CFLAGS += -mavx
$(objdir)/memcpy_t_avx_empty.o: CFLAGS += -mavx
$(objdir)/memset_t_avx_clflush.o: CFLAGS += -mavx
$(objdir)/memset_t_avx_clflushopt.o: CFLAGS += -mavx
$(objdir)/memset_t_avx_clwb.o: CFLAGS += -mavx
$(objdir)/memset_t_avx_empty.o: CFLAGS += -mavx

CFLAGS += -Ix86_64

ifeq ($(AVX512F_AVAILABLE), y)
CFLAGS += -DAVX512F_AVAILABLE=1
else
CFLAGS += -DAVX512F_AVAILABLE=0
endif
