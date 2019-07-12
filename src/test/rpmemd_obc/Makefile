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
# src/test/rpmemd_obc/Makefile -- build rpmemd_obc test
#
SCP_TO_REMOTE_NODES = y

vpath %.c ../../tools/rpmemd
vpath %.c ../../rpmem_common
vpath %.c ../../librpmem

TARGET = rpmemd_obc
OBJS = rpmemd_obc_test.o\
       rpmemd_obc_test_common.o\
       rpmemd_log.o\
       rpmem_common.o\
       rpmem_util.o\
       rpmem_ssh.o\
       rpmem_cmd.o\
       rpmemd_obc.o\
       rpmemd_obc_test_msg_hdr.o\
       rpmemd_obc_test_misc.o\
       rpmemd_obc_test_create.o\
       rpmemd_obc_test_open.o\
       rpmemd_obc_test_set_attr.o\
       rpmemd_obc_test_close.o

BIULD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCOMMON=y
include ../Makefile.inc

CFLAGS += -DRPMEMC_LOG_RPMEMD
INCS += -I../../tools/rpmemd
INCS += -I../../rpmem_common
INCS += -I../../librpmem
