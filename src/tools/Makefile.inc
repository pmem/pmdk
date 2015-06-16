# Copyright (c) 2014-2015, Intel Corporation
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
# src/tools/Makefile.inc -- Makefile include for all tools
#

INCS += -I. -I..
INCS += -I../..
INCS += -I../../include
CFLAGS += -std=gnu99 -Wall -Werror $(EXTRA_CFLAGS)
CFLAGS += -O2 -D_FORTIFY_SOURCE=2
CFLAGS += -DSRCVERSION='"$(SRCVERSION)"'
LDFLAGS += -Wl,-z,relro $(EXTRA_LDFLAGS)
TOOLSDIR=/usr/bin
TARGET_DIR=$(DESTDIR)$(TOOLSDIR)
BASH_COMP_FILES ?=
BASH_COMP_DIR =$(DESTDIR)/etc/bash_completion.d/
CSTYLE=../../../utils/cstyle

all: $(TARGET)

clean:
	$(RM) $(OBJS)

clobber: clean
	$(RM) $(TARGET)

cstyle:
	$(CSTYLE) -pP *.[ch]

install: all
ifneq ($(TARGET),)
	install -d $(TARGET_DIR)
	install -p -m 0755 $(TARGET) $(TARGET_DIR)
endif
ifneq ($(BASH_COMP_FILES),)
	install -d $(BASH_COMP_DIR)
	install -p -m 0644 $(BASH_COMP_FILES) $(BASH_COMP_DIR)
endif

uninstall:
ifneq ($(TARGET),)
	$(RM) $(TARGET_DIR)/$(TARGET)
endif
ifneq ($(BASH_COMP_FILES),)
	$(RM) $(BASH_COMP_DIR)/$(BASH_COMP_FILES)
endif


%.gz: %
	gzip -c ./$< > $@

%.txt: %
	man ./$< > $@

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

.PHONY: all clean clobber cstyle install uninstall
