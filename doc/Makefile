#
# Copyright (c) 2014, Intel Corporation
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
# doc/Makefile -- Makefile for NVM library man pages
#
MANPAGES = libpmem.3 libvmem.3
TXTFILES = $(MANPAGES:=.txt)
HTMLFILES = $(MANPAGES:=.html)
GZFILES = $(MANPAGES:=.gz)
MANPAGES_DESTDIR = $(DESTDIR)/usr/share/man/man3

all: $(TXTFILES)

%.3.txt: %.3
	man ./$< > $@

html: $(HTMLFILES)

%.3.html: %.3
	/usr/local/bin/groff -mandoc -Thtml ./$< > $@

compress: $(GZFILES)

%.3.gz: %.3
	gzip -c ./$< > $@

clean:

clobber: clean
	$(RM) -f $(TXTFILES) $(HTMLFILES) $(GZFILES)

install: compress
	install -d $(MANPAGES_DESTDIR)
	install -p -m 0644 $(GZFILES) $(MANPAGES_DESTDIR)

.PHONY: all html clean compress clobber cstyle install
