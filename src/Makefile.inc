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
# src/Makefile.inc -- common Makefile rules for NVM library
#

JEMALLOC_MAKE_NAME ?= Makefile
JEMALLOC_OBJROOT ?=
JEMALLOC_MAKE = $(JEMALLOC_DIR)/$(JEMALLOC_MAKE_NAME)
JEMALLOC_CFG = $(JEMALLOC_DIR)/configure
JEMALLOC_CFG_AC = $(JEMALLOC_DIR)/configure.ac
JEMALLOC_LIB_NAME = libjemalloc_pic.a
JEMALLOC_LIB = $(JEMALLOC_DIR)/$(JEMALLOC_OBJROOT)lib/$(JEMALLOC_LIB_NAME)

JECONFIG_FILE = ../jemalloc.cfg
JECONFIG = $(shell cat $(JECONFIG_FILE))

MACHINE := $(shell uname -m)

ifeq ($(MACHINE), x86_64)
ifneq ($(wildcard /usr/lib64),)
LIBDIR = usr/lib64
else
LIBDIR = usr/lib
endif
else
LIBDIR = usr/lib
endif

LIBS_DESTDIR = $(DESTDIR)/$(LIBDIR)/$(VARIANT_DESTDIR)

vpath %.c ..
vpath %.h .. ../include
INCS = -I.. -I../include

CFLAGS = -std=gnu99 -ggdb -Wall -Werror
LN = ln
TARGETS = libpmem.a libpmem.so libvmem.a libvmem.so
libpmem.a libpmem.so: LIBS += -luuid
COMMONOBJS = out.o util.o
PMEMOBJS = libpmem.o blk.o btt.o log.o pmem.o trn.o $(COMMONOBJS)
VMEMOBJS = libvmem.o vmem.o $(COMMONOBJS)
PMEMMAPFILE = ../libpmem.map
VMEMMAPFILE = ../libvmem.map
PMEMSOVERSION = 1
VMEMSOVERSION = 1
EXTRA_JECONFIG =
INSTALL_LINKS = libpmem.so.$(PMEMSOVERSION) libvmem.so.$(VMEMSOVERSION)

SRCVERSION := $(shell git describe --tags)
out.o: CFLAGS += -DSRCVERSION=\"$(SRCVERSION)\"

all: $(TARGETS)

jemalloc: $(JEMALLOC_LIB)

jeconfig $(JEMALLOC_MAKE): $(JEMALLOC_CFG) $(JECONFIG_FILE)
	cd $(JEMALLOC_DIR) && \
	    ./configure $(JECONFIG) $(EXTRA_JECONFIG)
	-mv -f $(JEMALLOC_DIR)/Makefile $(JEMALLOC_MAKE)

$(JEMALLOC_CFG): $(JEMALLOC_CFG_AC)
	cd $(JEMALLOC_DIR) && \
	    autoconf

$(VMEMOBJS): $(JEMALLOC_LIB)

$(JEMALLOC_LIB): $(JEMALLOC_MAKE)
	make objroot=$(JEMALLOC_OBJROOT) -f $(JEMALLOC_MAKE_NAME) -C $(JEMALLOC_DIR) all

libpmem.a: $(PMEMOBJS)
	$(LD) -o $*_unscoped.o -r $(PMEMOBJS)
	objcopy --localize-hidden `sed -n 's/^	*\([a-zA-Z0-9_]*\);$$/-G \1/p' $(PMEMMAPFILE)` $*_unscoped.o $*_all.o
	$(AR) rv $@ $*_all.o

libpmem.so: $(PMEMOBJS)
	$(CC) -shared -Wl,--version-script=$(PMEMMAPFILE),-soname,$@.$(PMEMSOVERSION) -o $@ $(PMEMOBJS) $(LIBS)
	$(RM) $@.$(PMEMSOVERSION)
	$(LN) -s $@ $@.$(PMEMSOVERSION)


libvmem.a libvmem.so: INCS += -I$(JEMALLOC_DIR)/include/jemalloc

libvmem.a: $(VMEMOBJS) $(JEMALLOC_LIB)
	$(LD) -o $*_unscoped.o -r $(VMEMOBJS) $(JEMALLOC_LIB)
	objcopy --localize-hidden `sed -n 's/^	*\([a-zA-Z0-9_]*\);$$/-G \1/p' $(VMEMMAPFILE)` $*_unscoped.o $*_all.o
	$(AR) rv $@ $*_all.o

libvmem.so: $(VMEMOBJS) $(JEMALLOC_LIB)
	$(CC) -shared -Wl,--version-script=$(VMEMMAPFILE),-soname,$@.$(VMEMSOVERSION) -o $@ $(VMEMOBJS) $(JEMALLOC_LIB)
	$(RM) $@.$(VMEMSOVERSION)
	$(LN) -s $@ $@.$(VMEMSOVERSION)

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(INCS) -fPIC $<

clean:
	$(RM) *.o core a.out
	@if [ -f $(JEMALLOC_MAKE) ];\
	then\
		make objroot=$(JEMALLOC_OBJROOT) -f $(JEMALLOC_MAKE_NAME) -C $(JEMALLOC_DIR) clean;\
	fi

clobber: clean
	$(RM) $(TARGETS) *.so.[0-9]*
	-make cfgoutputs_out+=$(JEMALLOC_MAKE) objroot=$(JEMALLOC_OBJROOT) -f $(JEMALLOC_MAKE_NAME) -C $(JEMALLOC_DIR) distclean
	rm -rf $(JEMALLOC_CFG)

install: $(TARGETS) $(INSTALL_LINKS)
	install -d $(LIBS_DESTDIR)
	install -p -m 0755 $(TARGETS) $(LIBS_DESTDIR)
	cp -d $(INSTALL_LINKS) $(LIBS_DESTDIR)

.PHONY: all clean clobber jeconfig jemalloc install

libpmem.o: libpmem.c libpmem.h pmem.h util.h out.h
blk.o: blk.c libpmem.h pmem.h blk.h util.h out.h
btt.o: btt.c util.h btt.h btt_layout.h
log.o: log.c libpmem.h pmem.h log.h util.h out.h
pmem.o: pmem.c libpmem.h pmem.h out.h
trn.o: trn.c libpmem.h pmem.h trn.h util.h out.h

libvmem.o: libvmem.c libvmem.h vmem.h util.h out.h
vmem.o: vmem.c libvmem.h util.h out.h

out.o: out.c out.h
util.o: util.c util.h out.h
