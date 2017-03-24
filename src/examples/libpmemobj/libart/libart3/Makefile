#
# Copyright 2017, FUJITSU TECHNOLOGY SOLUTIONS GMBH
# Copyright 2017, Intel Corporation
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
#                 andreas.bluemle@itxperts.de
#                 dieter.kasper@ts.fujitsu.com
#
#  Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
#
# ===========================================================================
#

PROGS = arttree3
LIBRARIES = art3

include ../../../Makefile.inc

INCDIR += -I$(TOP_SRC)/common -I$(TOP_SRC)/libpmemobj

# ## for optimized builds, uncomment the follwoing two lines
# LIBDIR = $(TOP_SRC)/nondebug
# CFLAGS += -O2

LIBS = -lpmemobj -pthread -lart3

$(PROGS): | $(DYNAMIC_LIBRARIES)
$(PROGS): LDFLAGS += -Wl,-rpath=. -L.

libart3.o: art.o

arttree3: arttree.o

clean-libraries:
	$(RM) *.so *.a

clean-progs:
	$(RM) $(PROGS) *.o
