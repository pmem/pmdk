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
# src/libvmem/jemalloc.inc -- rules for jemalloc
#

JEMALLOC_DIR = $(realpath ../jemalloc)
ifeq ($(OBJDIR),$(abspath $(OBJDIR)))
JEMALLOC_OBJDIR = $(OBJDIR)/jemalloc
else
JEMALLOC_OBJDIR = ../$(OBJDIR)/jemalloc
endif
JEMALLOC_MAKEFILE = $(JEMALLOC_OBJDIR)/Makefile
JEMALLOC_CFG = $(JEMALLOC_DIR)/configure
JEMALLOC_CFG_AC = $(JEMALLOC_DIR)/configure.ac
JEMALLOC_LIB_AR = libjemalloc_pic.a
JEMALLOC_LIB = $(JEMALLOC_OBJDIR)/lib/$(JEMALLOC_LIB_AR)
JEMALLOC_CFG_IN_FILES = $(shell find $(JEMALLOC_DIR) -name "*.in")
JEMALLOC_CFG_GEN_FILES = $(JEMALLOC_CFG_IN_FILES:.in=)
JEMALLOC_CFG_OUT_FILES = $(patsubst $(JEMALLOC_DIR)/%, $(JEMALLOC_OBJDIR)/%, $(JEMALLOC_CFG_GEN_FILES))
.NOTPARALLEL: $(JEMALLOC_CFG_OUT_FILES)
JEMALLOC_CONFIG_FILE = jemalloc.cfg
JEMALLOC_CONFIG = $(shell cat $(JEMALLOC_CONFIG_FILE))

jemalloc $(JEMALLOC_LIB): $(JEMALLOC_CFG_OUT_FILES)
	$(MAKE) objroot=$(JEMALLOC_OBJDIR)/ -f $(JEMALLOC_MAKEFILE) -C $(JEMALLOC_DIR) all

$(JEMALLOC_CFG_OUT_FILES): $(JEMALLOC_CFG) $(JEMALLOC_CONFIG_FILE)
	$(MKDIR) -p $(JEMALLOC_OBJDIR)
	cd $(JEMALLOC_OBJDIR) && \
		CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" $(JEMALLOC_DIR)/configure $(JEMALLOC_CONFIG)

$(JEMALLOC_CFG): $(JEMALLOC_CFG_AC)
	cd $(JEMALLOC_DIR) && \
	    autoconf

jemalloc-clean:
	@if [ -f $(JEMALLOC_MAKEFILE) ];\
	then\
		$(MAKE) cfgoutputs_out+=$(JEMALLOC_MAKEFILE) objroot=$(JEMALLOC_OBJDIR)/ -f $(JEMALLOC_MAKEFILE) -C $(JEMALLOC_DIR) clean;\
	fi

jemalloc-clobber:
	@if [ -f $(JEMALLOC_MAKEFILE) ];\
	then\
		$(MAKE) cfgoutputs_out+=$(JEMALLOC_MAKEFILE) objroot=$(JEMALLOC_OBJDIR)/ -f $(JEMALLOC_MAKEFILE) -C $(JEMALLOC_DIR) distclean;\
	fi
	$(RM) $(JEMALLOC_CFG) $(JEMALLOC_CFG_GEN_FILES) $(JEMALLOC_CFG_OUT_FILES)
	$(RM) -r $(JEMALLOC_OBJDIR)

jemalloc-test: jemalloc
	$(MAKE) objroot=$(JEMALLOC_OBJDIR)/ -f $(JEMALLOC_MAKEFILE) -C $(JEMALLOC_DIR) tests

jemalloc-check: jemalloc-test
	$(MAKE) objroot=$(JEMALLOC_OBJDIR)/ -f $(JEMALLOC_MAKEFILE) -C $(JEMALLOC_DIR) check

.PHONY: jemalloc jemalloc-clean jemalloc-clobber jemalloc-test jemalloc-check
