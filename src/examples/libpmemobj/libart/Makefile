#
# Copyright (c) 2016, FUJITSU TECHNOLOGY SOLUTIONS GMBH
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
#     * Neither the name of Fujitsu Technology Solutions GmbH nor the names
#       of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
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

PROGS =
ARTTREE = arttree
ANALYZE = arttree_structures
ANALYZE_OBJS = arttree_structures.o arttree_examine.o arttree_search.o
LIBART_SO = libart

INCDIR ?= ../../../include
LIBDIR ?= ../../../debug

INCS = -I$(INCDIR)
LIBS = -Wl,-rpath=$(LIBDIR) -L$(LIBDIR) -lpmemobj -pthread
LIBART = -Wl,-rpath=. -L. -lart
CFLAGS = -std=gnu99 -DDEBUG -ggdb -Wall -Werror -fPIC
CSTYLE = ../../../../utils/cstyle
LDFLAGS += -Wl,-z,relro -Wl,--fatal-warnings -Wl,--warn-common $(EXTRA_LDFLAGS)

all:     TARGET = all
clean:   TARGET = clean
clobber: TARGET = clobber
cstyle:  TARGET = cstyle

all: $(PROGS) $(ANALYZE) $(ARTTREE)

clean: $(DIRS)
	$(RM) *.o *.so $(PROGS) $(ANALYZE) $(ARTTREE) core a.out

clobber: clean $(DIRS)
	$(RM) $(PROGS)

cstyle: $(DIRS)
	$(CSTYLE) -pP *.[ch]

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(INCS) $<

$(PROGS):
	$(CC) -o $@ $< $(LIBS)

$(ANALYZE): $(ANALYZE_OBJS)
	$(CC) -o $@ $(ANALYZE_OBJS) $(LIBS)

$(LIBART_SO): art.o
	$(CC) $(LDFLAGS) -shared -o $(LIBART_SO).so art.o

var_strings: var_strings.o

unions: unions.o

$(ARTTREE): $(ARTTREE).o $(LIBART_SO)
	$(CC) -o $@ $(ARTTREE).o $(LIBS) $(LIBART)

$(DIRS):
	$(MAKE) -C $@ $(TARGET)

.PHONY: all clean clobber cstyle $(DIRS)
