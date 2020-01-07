# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
# Copyright 2016, FUJITSU TECHNOLOGY SOLUTIONS GMBH
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
# ===========================================================================
#
#      Filename:  Makefile
#
#   Description:  build libart port to libpmemobj
#
#        Author:  Andreas Bluemle, Dieter Kasper
#                 Andreas.Bluemle.external@ts.fujitsu.com
#                 dieter.kasper@ts.fujitsu.com
#
#  Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
#
# ===========================================================================
#

include ../../../common.inc

ifeq ($(ARCH), x86_64)
# libart uses x86 intrinsics
PROGS = arttree examine_arttree
LIBRARIES = art
endif

LIBS = -lpmemobj

include ../../Makefile.inc

$(PROGS): | $(DYNAMIC_LIBRARIES)
$(PROGS): LDFLAGS += -Wl,-rpath=. -L. -lart

libart.o: art.o

arttree: arttree.o
examine_arttree: arttree_structures.o arttree_examine.o arttree_search.o
