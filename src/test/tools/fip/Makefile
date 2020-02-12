# Copyright 2016-2020, Intel Corporation
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
# Makefile -- Makefile for fip tool
#

TOP = ../../../..
include $(TOP)/src/common.inc

vpath %.c $(TOP)/src/rpmem_common/
vpath %.c $(TOP)/src/core/

ifeq ($(BUILD_RPMEM),y)
SCP_TO_REMOTE_NODES = y
TARGET = fip

OBJS = fip.o rpmem_fip_common.o rpmem_common.o util.o os_posix.o alloc.o

else
$(info NOTE: Skipping fip because $(BUILD_RPMEM_INFO))
endif

include $(TOP)/src/tools/Makefile.inc

ifeq ($(BUILD_RPMEM),y)
CFLAGS += -I$(TOP)/src/rpmem_common
CFLAGS += -I$(TOP)/src/core
CFLAGS += $(shell $(PKG_CONFIG) --cflags libfabric)
LIBS += $(shell $(PKG_CONFIG) --libs libfabric)
endif
