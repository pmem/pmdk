#
# Copyright 2014-2017, Intel Corporation
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
# doc/Makefile -- Makefile for NVM library man pages
#

include ../src/common.inc

MANPAGES_7_MD = libpmem/libpmem.7.md libpmemblk/libpmemblk.7.md libpmemlog/libpmemlog.7.md \
		libpmemobj/libpmemobj.7.md libpmempool/libpmempool.7.md libvmem/libvmem.7.md \
		libvmmalloc/libvmmalloc.7.md

MANPAGES_5_MD = poolset/poolset.5.md

MANPAGES_3_MD = libpmem/pmem_flush.3.md libpmem/pmem_is_pmem.3.md libpmem/pmem_memmove_persist.3.md \
		libpmemblk/pmemblk_bsize.3.md libpmemblk/pmemblk_create.3.md libpmemblk/pmemblk_read.3.md libpmemblk/pmemblk_set_zero.3.md \
		libpmemlog/pmemlog_append.3.md libpmemlog/pmemlog_create.3.md libpmemlog/pmemlog_nbyte.3.md libpmemlog/pmemlog_tell.3.md \
		libpmemobj/oid_is_null.3.md libpmemobj/pmemobj_alloc.3.md libpmemobj/pmemobj_ctl_get.3.md libpmemobj/pmemobj_first.3.md \
		libpmemobj/pmemobj_list_insert.3.md libpmemobj/pmemobj_memcpy_persist.3.md libpmemobj/pmemobj_mutex_zero.3.md \
		libpmemobj/pmemobj_open.3.md libpmemobj/pmemobj_root.3.md libpmemobj/pmemobj_tx_begin.3.md libpmemobj/pobj_layout_begin.3.md \
		libpmemobj/pobj_list_head.3.md libpmemobj/toid_declare.3.md \
		libpmempool/pmempool_check_init.3.md libpmempool/pmempool_rm.3.md libpmempool/pmempool_sync.3.md \
		libvmem/vmem_create.3.md libvmem/vmem_malloc.3.md

MANPAGES_1_MD = pmempool/pmempool.1.md pmempool/pmempool-info.1.md pmempool/pmempool-create.1.md \
		pmempool/pmempool-check.1.md pmempool/pmempool-dump.1.md pmempool/pmempool-rm.1.md \
		pmempool/pmempool-convert.1.md pmempool/pmempool-sync.1.md pmempool/pmempool-transform.1.md

MANPAGES_BUILDDIR = generated


ifeq ($(BUILD_RPMEM),y)
MANPAGES_7_MD += librpmem/librpmem.7.md
MANPAGES_3_MD += librpmem/rpmem_create.3.md
MANPAGES_3_MD += librpemm/rpmem_persist.3.md
MANPAGES_1_MD += rpmemd/rpmemd.1.md
endif

MANPAGES_7_GROFF = $(MANPAGES_7_MD:.7.md=.7)
MANPAGES_5_GROFF = $(MANPAGES_5_MD:.5.md=.5)
MANPAGES_3_GROFF = $(MANPAGES_3_MD:.3.md=.3)
MANPAGES_1_GROFF = $(MANPAGES_1_MD:.1.md=.1)

MANPAGES_7_GROFF_EXP = $(MANPAGES_7_MD_EXP:.3.md=.7)
MANPAGES_5_GROFF_EXP = $(MANPAGES_5_MD_EXP:.3.md=.5)
MANPAGES_3_GROFF_EXP = $(MANPAGES_3_MD_EXP:.3.md=.3)
MANPAGES_1_GROFF_EXP = $(MANPAGES_1_MD_EXP:.1.md=.1)

ifeq ($(EXPERIMENTAL),y)
$(MANPAGES_7_GROFF) += $(MANPAGES_7_GROFF_EXP)
$(MANPAGES_5_GROFF) += $(MANPAGES_5_GROFF_EXP)
$(MANPAGES_3_GROFF) += $(MANPAGES_3_GROFF_EXP)
$(MANPAGES_1_GROFF) += $(MANPAGES_1_GROFF_EXP)
else
MANPAGES_7_NOINSTALL += $(MANPAGES_7_GROFF_EXP)
MANPAGES_5_NOINSTALL += $(MANPAGES_5_GROFF_EXP)
MANPAGES_3_NOINSTALL += $(MANPAGES_3_GROFF_EXP)
MANPAGES_1_NOINSTALL += $(MANPAGES_1_GROFF_EXP)
endif

MANPAGES = $(MANPAGES_7_GROFF) $(MANPAGES_5_GROFF) $(MANPAGES_3_GROFF) $(MANPAGES_1_GROFF) \
	   $(MANPAGES_7_NOINSTALL) $(MANPAGES_5_NOINSTALL) $(MANPAGES_3_NOINSTALL) $(MANPAGES_1_NOINSTALL)

MANPAGES_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(MANPAGES)))
MANPAGES_BUILD_DIR = $(addprefix $(MANPAGES_BUILDDIR)/, $(MANPAGES))

HTMLFILES = $(MANPAGES_BUILD:=.html)
TXTFILES = $(MANPAGES_BUILD:=.txt)

GZFILES_7 = $(MANPAGES_7_GROFF:=.gz)
GZFILES_5 = $(MANPAGES_5_GROFF:=.gz)
GZFILES_3 = $(MANPAGES_3_GROFF:=.gz)
GZFILES_1 = $(MANPAGES_1_GROFF:=.gz)
GZFILES_7_NOINSTALL = $(MANPAGES_7_NOINSTALL:=.gz)
GZFILES_5_NOINSTALL = $(MANPAGES_5_NOINSTALL:=.gz)
GZFILES_3_NOINSTALL = $(MANPAGES_3_NOINSTALL:=.gz)
GZFILES_1_NOINSTALL = $(MANPAGES_1_NOINSTALL:=.gz)

GZFILES = $(GZFILES_7) $(GZFILES_5) $(GZFILES_3) $(GZFILES_1) \
	  $(GZFILES_7_NOINSTALL) $(GZFILES_5_NOINSTALL) $(GZFILES_3_NOINSTALL) $(GZFILES_1_NOINSTALL)

GZFILES_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES)))
GZFILES_7_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_7)))
GZFILES_5_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_5)))
GZFILES_3_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_3)))
GZFILES_1_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_1)))

DOXYGEN_HTMLDIR=cpp_html

MANPAGES_DESTDIR_7 = $(DESTDIR)$(man7dir)
MANPAGES_DESTDIR_5 = $(DESTDIR)$(man5dir)
MANPAGES_DESTDIR_3 = $(DESTDIR)$(man3dir)
MANPAGES_DESTDIR_1 = $(DESTDIR)$(man1dir)

DOCS_DESTDIR = $(DESTDIR)$(docdir)
CPP_DOC_DIR ?= libpmemobj++-dev
CPP_DOCS_DESTDIR = $(DOCS_DESTDIR)/$(CPP_DOC_DIR)


all: md2man $(TXTFILES) doxygen_docs | $(MANPAGES_BUILDDIR)

$(MANPAGES_BUILDDIR):
	$(MKDIR) -p $@

%.txt: %
	man ./$< > $@

groff: $(MANPAGES_7_GROFF) $(MANPAGES_5_GROFF) $(MANPAGES_3_GROFF) $(MANPAGES_1_GROFF)

doxygen_docs:
	doxygen cppobj.Doxyfile

html: $(HTMLFILES) doxygen_docs

%.html: %
	groff -mandoc -Thtml ./$< > $@

md2man:
	$(foreach f, $(MANPAGES_7_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_5_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_3_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_1_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)


#$(MANPAGES_BUILDDIR)%.1: $(MANPAGES_1_MD) default.man macros.man ../utils/md2man.sh FORCE
#	../utils/md2man.sh ./$< default.main $@

#$(MANPAGES_BUILDDIR)/%.3: $(MANPAGES_3_MD) default.man macros.man ../utils/md2man.sh FORCE
#	../utils/md2man.sh ./$< default.man $@

#$(MANPAGES_BUILDDIR)/%.5: $(MANPAGES_5_MD) default.man macros.man ../utils/md2man.sh FORCE
#	../utils/md2man.sh ./$< default.man $@
#$(MANPAGES_BUILDDIR)/%.7: $(MANPAGES_7_MD) default.man macros.man ../utils/md2man.sh FORCE
#	../utils/md2man.sh  default.man $@

compress: $(GZFILES_BUILD)

%.gz:
	gzip -c ./$* > $@

clean:

clobber: clean
	$(RM) -rf $(DOXYGEN_HTMLDIR) \
		$(MANPAGES_BUILDDIR)/*.txt \
		$(MANPAGES_BUILDDIR)/*.html \
		$(MANPAGES_BUILDDIR)/*.gz

install-cpp: doxygen_docs
	$(call install_recursive,$(DOXYGEN_HTMLDIR),0644,$(CPP_DOCS_DESTDIR))

install: install-cpp

uninstall-cpp:
	$(RM) -rf $(CPP_DOCS_DESTDIR)

uninstall: uninstall-cpp

install: compress
	install -d -v $(MANPAGES_DESTDIR_7)
	install -p -m 0644 $(GZFILES_7_BUILD) $(MANPAGES_DESTDIR_7)
	install -d -v $(MANPAGES_DESTDIR_5)
	install -p -m 0644 $(GZFILES_5_BUILD) $(MANPAGES_DESTDIR_5)
	install -d -v $(MANPAGES_DESTDIR_3)
	install -p -m 0644 $(GZFILES_3_BUILD) $(MANPAGES_DESTDIR_3)
	install -d -v $(MANPAGES_DESTDIR_1)
	install -p -m 0644 $(GZFILES_1_BUILD) $(MANPAGES_DESTDIR_1)

uninstall:
	$(foreach f, $(notdir $(GZFILES_7_BUILD)), $(RM) $(MANPAGES_DESTDIR_7)/$(f))
	$(foreach f, $(notdir $(GZFILES_5_BUILD)), $(RM) $(MANPAGES_DESTDIR_5)/$(f))
	$(foreach f, $(notdir $(GZFILES_3_BUILD)), $(RM) $(MANPAGES_DESTDIR_3)/$(f))
	$(foreach f, $(notdir $(GZFILES_1_BUILD)), $(RM) $(MANPAGES_DESTDIR_1)/$(f))

FORCE:

.PHONY: all html clean compress clobber cstyle install uninstall install-cpp\
	uninstall-cpp doxygen_docs
