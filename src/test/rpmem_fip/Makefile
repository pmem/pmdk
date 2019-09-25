#
# Copyright 2016-2019, Intel Corporation
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
# src/test/rpmem_fip/Makefile -- build rpmem_fip test
#

include ../../common.inc

vpath %.c ../../librpmem/
vpath %.c ../../rpmem_common/
vpath %.c ../../tools/rpmemd

ifeq ($(BUILD_RPMEM), y)
SCP_TO_REMOTE_NODES = y
TARGET = rpmem_fip
OBJS = rpmem_fip_test.o\
       rpmem_fip_oob.o\
       rpmem_fip.o\
       rpmemd_fip.o\
       rpmem_util.o\
       rpmem_ssh.o\
       rpmem_cmd.o\
       rpmem_common.o\
       rpmem_fip_common.o\
       rpmemd_util.o\
       rpmemd_obc.o\
       rpmemd_log.o

endif

BUILD_STATIC_DEBUG=n
BUILD_STATIC_RELEASE=n

LIBPMEMCOMMON=y

include ../Makefile.inc

ifeq ($(BUILD_RPMEM), y)
LIBS += $(shell $(PKG_CONFIG) --libs libfabric)
CFLAGS += $(shell $(PKG_CONFIG) --cflags libfabric)
CFLAGS += -DDEBUG
CFLAGS += -DRPMEMC_LOG_RPMEM
INCS += -I../../librpmem/
INCS += -I../../rpmem_common/
INCS += -I../../tools/rpmemd
endif
