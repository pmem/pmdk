#
# Copyright (c) 2015, Intel Corporation
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
#     * Neither the name of Intel Corporation nor the names of its
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
# examples/Makefile.inc -- build the NVM Library examples
#
TOP_SRC := ../$(dir $(lastword $(MAKEFILE_LIST)))
TOP := ../$(TOP_SRC)
CSTYLE = $(TOP)utils/cstyle

INCDIR ?= $(TOP_SRC)include
LIBDIR ?= $(TOP_SRC)debug

CFLAGS = -std=gnu99 -ggdb -Wall -Werror
LDFLAGS = -Wl,-rpath=$(LIBDIR) -L$(LIBDIR)
INCS = -I$(INCDIR)

all-dirs:     TARGET = all
clean-dirs:   TARGET = clean
clobber-dirs: TARGET = clobber
cstyle-dirs:  TARGET = cstyle

all: $(if $(DIRS), all-dirs, ) $(if $(PROGS), all-progs, )
clean: $(if $(DIRS), clean-dirs, ) $(if $(PROGS), clean-progs, )
clobber: $(if $(DIRS), clobber-dirs, ) $(if $(PROGS), clobber-progs, )
cstyle: $(if $(DIRS), cstyle-dirs, ) $(if $(PROGS), cstyle-progs, )

all-dirs clean-dirs clobber-dirs cstyle-dirs: $(DIRS)
all-progs: $(PROGS)

$(DIRS):
	$(MAKE) -C $@ $(TARGET)

clobber-progs: clean-progs
	$(RM) $(PROGS)

clean-progs:
	$(RM) *.o

cstyle-progs:
	$(CSTYLE) -pP *.[ch]

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(INCS) $<

$(PROGS):
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

.PHONY: all clean clobber cstyle\
	all-dirs clean-dirs clobber-dirs cstyle-dirs\
	all-progs clean-progs clobber-progs cstyle-progs\
	$(DIRS)
