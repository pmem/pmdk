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
# src/test/rpmem_obc/Makefile -- build rpmem_obc test
#
vpath %.c ../../librpmem/
vpath %.c ../../rpmem_common/
vpath %.c ../../common/
vpath %.c ../../../platform/linux/src/common

TARGET = rpmem_obc
OBJS = rpmem_obc_test.o\
       rpmem_obc_test_common.o\
       rpmem_obc_test_misc.o\
       rpmem_obc_test_create.o\
       rpmem_obc_test_open.o\
       rpmem_obc_test_close.o\
       rpmem_obc_test_remove.o\
       rpmem_obc.o rpmem_util.o rpmem_common.o\
       out.o util.o util_linux.o

out.o: CFLAGS += -DDEBUG -DSRCVERSION=\"utversion\"

include ../Makefile.inc

CFLAGS += -DRPMEMC_LOG_RPMEM
INCS += -I../../librpmem/
INCS += -I../../rpmem_common/
INCS += -I../../common
