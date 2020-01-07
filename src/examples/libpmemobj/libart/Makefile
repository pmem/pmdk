#
# Copyright 2016, FUJITSU TECHNOLOGY SOLUTIONS GMBH
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation

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
