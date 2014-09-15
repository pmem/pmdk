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

JEMALLOC_OBJROOT ?=
JEMALLOC_MAKE_NAME = $(JEMALLOC_OBJROOT)/Makefile
JEMALLOC_MAKE = $(JEMALLOC_DIR)/$(JEMALLOC_MAKE_NAME)
JEMALLOC_CFG = $(JEMALLOC_DIR)/configure
JEMALLOC_CFG_AC = $(JEMALLOC_DIR)/configure.ac
JEMALLOC_LIB_NAME = libjemalloc_pic.a
JEMALLOC_LIB = $(JEMALLOC_DIR)/$(JEMALLOC_OBJROOT)/lib/$(JEMALLOC_LIB_NAME)

JEMALLOC_CFG_IN_FILES = $(shell find $(JEMALLOC_DIR) -name "*.in")
JEMALLOC_CFG_GEN_FILES = $(JEMALLOC_CFG_IN_FILES:.in=)
JEMALLOC_CFG_OUT_FILES = $(patsubst $(JEMALLOC_DIR)/%, $(JEMALLOC_DIR)/$(JEMALLOC_OBJROOT)/%, $(JEMALLOC_CFG_GEN_FILES))
JEMALLOC_CFG_OUT_FILES_REL = $(patsubst $(JEMALLOC_DIR)/%, %, $(JEMALLOC_CFG_GEN_FILES))
.NOTPARALLEL: $(JEMALLOC_CFG_OUT_FILES)

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
LDFLAGS = -Wl,-z,relro

LN = ln
PMEMSOVERSION = 1
VMEMSOVERSION = 1
PMEMLIBVERSION = 0.0
VMEMLIBVERSION = 0.0
LIBPMEMSO=libpmem.so
LIBVMEMSO=libvmem.so
LIBPMEMAR=libpmem.a
LIBVMEMAR=libvmem.a
LIBPMEM_SONAME=$(LIBPMEMSO).$(PMEMSOVERSION)
LIBVMEM_SONAME=$(LIBVMEMSO).$(VMEMSOVERSION)
LIBPMEM_REALNAME=$(LIBPMEM_SONAME).$(PMEMLIBVERSION)
LIBVMEM_REALNAME=$(LIBVMEM_SONAME).$(VMEMLIBVERSION)

COMMONOBJS = out.o util.o
PMEMOBJS = libpmem.o blk.o btt.o log.o pmem.o trn.o $(COMMONOBJS)
VMEMOBJS = libvmem.o vmem.o $(COMMONOBJS)
PMEMMAPFILE = ../libpmem.map
VMEMMAPFILE = ../libvmem.map
TARGET_LIBS = $(LIBPMEMAR) $(LIBPMEM_REALNAME) $(LIBVMEMAR) $(LIBVMEM_REALNAME)
TARGET_LINKS= $(LIBPMEMSO) $(LIBPMEM_SONAME) $(LIBVMEMSO) $(LIBVMEM_SONAME)
TARGETS = $(TARGET_LIBS) $(TARGET_LINKS)
EXTRA_JECONFIG =

$(LIBPMEM_AR) $(LIBPMEM_REALNAME): LIBS += -luuid

out.o: CFLAGS += -DSRCVERSION='"$(SRCVERSION)"'

all: $(TARGETS)

jemalloc: $(JEMALLOC_LIB)

jeconfig $(JEMALLOC_CFG_OUT_FILES): $(JEMALLOC_CFG) $(JECONFIG_FILE)
	cd $(JEMALLOC_DIR) && \
	CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" ./configure $(JECONFIG) $(EXTRA_JECONFIG)
	@for FILE in $(JEMALLOC_CFG_OUT_FILES_REL);\
	do\
		mkdir -p $$(dirname $(JEMALLOC_DIR)/$(JEMALLOC_OBJROOT)/$$FILE);\
		mv -vf $(JEMALLOC_DIR)/$$FILE $(JEMALLOC_DIR)/$(JEMALLOC_OBJROOT)/$$FILE;\
	done

$(JEMALLOC_CFG): $(JEMALLOC_CFG_AC)
	cd $(JEMALLOC_DIR) && \
	    autoconf

$(VMEMOBJS): $(JEMALLOC_LIB)

$(JEMALLOC_LIB): $(JEMALLOC_CFG_OUT_FILES)
	make objroot=$(JEMALLOC_OBJROOT)/ -f $(JEMALLOC_MAKE_NAME) -C $(JEMALLOC_DIR) all

jemalloc-tests: jemalloc
	make objroot=$(JEMALLOC_OBJROOT)/ -f $(JEMALLOC_MAKE_NAME) -C $(JEMALLOC_DIR) tests

jemalloc-check: jemalloc-tests
	make objroot=$(JEMALLOC_OBJROOT)/ -f $(JEMALLOC_MAKE_NAME) -C $(JEMALLOC_DIR) check

$(LIBPMEMAR): $(PMEMOBJS)
	$(LD) -o $*_unscoped.o -r $(PMEMOBJS)
	objcopy --localize-hidden `sed -n 's/^	*\([a-zA-Z0-9_]*\);$$/-G \1/p' $(PMEMMAPFILE)` $*_unscoped.o $*_all.o
	$(AR) rv $@ $*_all.o

$(LIBPMEM_REALNAME): $(PMEMOBJS)
	$(CC) $(LDFLAGS) -shared -Wl,--version-script=$(PMEMMAPFILE),-soname,$(LIBPMEM_SONAME) -o $@ $(PMEMOBJS) $(LIBS)

$(LIBPMEM_SONAME): $(LIBPMEM_REALNAME)
	$(LN) -sf $< $@

$(LIBPMEMSO): $(LIBPMEM_SONAME)
	$(LN) -s $< $@


$(LIBVMEMAR) $(LIBVMEM_REALNAME): INCS += -I$(JEMALLOC_DIR)/include/jemalloc

$(LIBVMEMAR): $(VMEMOBJS) $(JEMALLOC_LIB)
	$(LD) -o $*_unscoped.o -r $(VMEMOBJS) $(JEMALLOC_LIB)
	objcopy --localize-hidden `sed -n 's/^	*\([a-zA-Z0-9_]*\);$$/-G \1/p' $(VMEMMAPFILE)` $*_unscoped.o $*_all.o
	$(AR) rv $@ $*_all.o

$(LIBVMEM_REALNAME): $(VMEMOBJS) $(JEMALLOC_LIB)
	$(CC) $(LDFLAGS) -shared -Wl,--version-script=$(VMEMMAPFILE),-soname,$(LIBVMEM_SONAME) -o $@ $(VMEMOBJS) $(JEMALLOC_LIB)

$(LIBVMEM_SONAME): $(LIBVMEM_REALNAME)
	$(LN) -sf $< $@

$(LIBVMEMSO): $(LIBVMEM_SONAME)
	$(LN) -s $< $@

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(INCS) -fPIC $<

clean:
	$(RM) *.o core a.out
	@if [ -f $(JEMALLOC_MAKE) ];\
	then\
		make objroot=$(JEMALLOC_OBJROOT)/ -f $(JEMALLOC_MAKE_NAME) -C $(JEMALLOC_DIR) clean;\
	fi

clobber: clean
	$(RM) $(TARGETS)
	@if [ -f $(JEMALLOC_MAKE) ];\
	then\
		make cfgoutputs_out+=$(JEMALLOC_MAKE) objroot=$(JEMALLOC_OBJROOT)/ -f $(JEMALLOC_MAKE_NAME) -C $(JEMALLOC_DIR) distclean;\
	fi
	$(RM) $(JEMALLOC_CFG)
	$(RM) -r $(JEMALLOC_CFG_GEN_FILES)
	$(RM) -r $(JEMALLOC_CFG_OUT_FILES)
	$(RM) -r $(JEMALLOC_DIR)/$(JEMALLOC_OBJROOT)

install: $(TARGETS)
	install -d $(LIBS_DESTDIR)
	install -p -m 0755 $(TARGET_LIBS) $(LIBS_DESTDIR)
	cp -d $(TARGET_LINKS) $(LIBS_DESTDIR)

.PHONY: all clean clobber jeconfig jemalloc install jemalloc-tests jemalloc-check

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
