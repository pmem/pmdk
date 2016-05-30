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
# src/librpmem/Makefile -- Makefile for librpmem
#
include ../common.inc

vpath %.c ../rpmem_common

ifeq ($(call check_package, libfabric),y)
LIBRARY_NAME = rpmem
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0
SOURCE = $(COMMON)/util.c\
	$(COMMON)/util_linux.c\
	$(COMMON)/out.c\
	librpmem.c\
	rpmem.c\
	rpmem_obc.c\
	rpmem_common.c\
	rpmem_util.c\
	rpmem_fip_common.c\
	rpmem_fip.c
else
$(info NOTE: Skipping librpmem because libfabric is missing \
-- see src/librpmem/README for details.)
endif

include ../Makefile.inc

CFLAGS += -I. -I../rpmem_common
CFLAGS += -DRPMEMC_LOG_RPMEM
