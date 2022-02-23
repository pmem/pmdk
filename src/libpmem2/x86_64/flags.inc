# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2022, Intel Corporation

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

$(objdir)/memcpy_nt_movdir64b.o: CFLAGS += -mavx512f -mmovdir64b
$(objdir)/memset_nt_movdir64b.o: CFLAGS += -mavx512f -mmovdir64b

$(objdir)/memcpy_t_avx512f.o: CFLAGS += -mavx512f
$(objdir)/memset_t_avx512f.o: CFLAGS += -mavx512f

$(objdir)/memcpy_t_avx.o: CFLAGS += -mavx
$(objdir)/memset_t_avx.o: CFLAGS += -mavx

$(objdir)/memcpy_t_movdir64b.o: CFLAGS += -mavx512f -mmovdir64b
$(objdir)/memset_t_movdir64b.o: CFLAGS += -mavx512f -mmovdir64b

CFLAGS += -I$(TOP)/src/libpmem2/x86_64

ifeq ($(AVX512F_AVAILABLE), y)
CFLAGS += -DAVX512F_AVAILABLE=1
else
CFLAGS += -DAVX512F_AVAILABLE=0
endif

ifeq ($(MOVDIR64B_AVAILABLE), y)
CFLAGS += -DMOVDIR64B_AVAILABLE=1
else
CFLAGS += -DMOVDIR64B_AVAILABLE=0
endif
