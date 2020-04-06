# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation

#
# src/libpmem2/x86_64/flags.inc -- flags for libpmem2/x86_64
#

vpath %.c $(TOP)/src/libpmem2/x86_64
vpath %.h $(TOP)/src/libpmem2/x86_64
vpath %.c $(TOP)/src/libpmem2/x86_64/memcpy
vpath %.c $(TOP)/src/libpmem2/x86_64/memset

$(objdir)/memcpy_nt_avx512f.o: CFLAGS += -mavx512f
$(objdir)/memset_nt_avx512f.o: CFLAGS += -mavx512f

$(objdir)/memcpy_nt_avx.o: CFLAGS += -mavx
$(objdir)/memset_nt_avx.o: CFLAGS += -mavx

$(objdir)/memcpy_t_avx512f.o: CFLAGS += -mavx512f
$(objdir)/memset_t_avx512f.o: CFLAGS += -mavx512f

$(objdir)/memcpy_t_avx.o: CFLAGS += -mavx
$(objdir)/memset_t_avx.o: CFLAGS += -mavx

CFLAGS += -I$(TOP)/src/libpmem2/x86_64

ifeq ($(AVX512F_AVAILABLE), y)
CFLAGS += -DAVX512F_AVAILABLE=1
else
CFLAGS += -DAVX512F_AVAILABLE=0
endif
