#
# Copyright 2014-2018, Intel Corporation
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
# doc/Makefile -- Makefile for PMDK man pages
#

include ../src/common.inc

MANPAGES_7_MD = libvmem/libvmem.7.md \
		libvmmalloc/libvmmalloc.7.md

MANPAGES_5_MD = poolset/poolset.5.md pmem_ctl/pmem_ctl.5.md

MANPAGES_3_MD =	libvmem/vmem_create.3.md libvmem/vmem_malloc.3.md

MANPAGES_1_MD =

MANPAGES_3_DUMMY = vmem_create_in_region.3 vmem_delete.3 vmem_check.3 vmem_stats_print.3 \
		   vmem_calloc.3 vmem_realloc.3 vmem_free.3 vmem_aligned_alloc.3 vmem_strdup.3 vmem_wcsdup.3 vmem_malloc_usable_size.3 \
		   vmem_check_version.3 vmem_errormsg.3 vmem_set_funcs.3

MANPAGES_BUILDDIR = generated
MANPAGES_WEBDIR_LINUX = web_linux
MANPAGES_WEBDIR_WINDOWS = web_windows


# experimental
MANPAGES_3_MD_EXP =
MANPAGES_1_MD_EXP =

MANPAGES_3_MD_DUMMY_EXP =
MANPAGES_1_MD_DUMMY_EXP =

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
GZFILES_3_DUMMY = $(MANPAGES_3_DUMMY:=.gz)

GZFILES = $(GZFILES_7) $(GZFILES_5) $(GZFILES_3) $(GZFILES_1) \
	  $(GZFILES_7_NOINSTALL) $(GZFILES_5_NOINSTALL) $(GZFILES_3_NOINSTALL) $(GZFILES_1_NOINSTALL) \
	  $(GZFILES_3_DUMMY)

GZFILES_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES)))
GZFILES_7_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_7)))
GZFILES_5_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_5)))
GZFILES_3_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_3)))
GZFILES_1_BUILD = $(addprefix $(MANPAGES_BUILDDIR)/, $(notdir $(GZFILES_1)))

GZFILES_3_BUILD += $(addprefix $(MANPAGES_BUILDDIR)/, $(GZFILES_3_DUMMY))

MANPAGES_DESTDIR_7 = $(DESTDIR)$(man7dir)
MANPAGES_DESTDIR_5 = $(DESTDIR)$(man5dir)
MANPAGES_DESTDIR_3 = $(DESTDIR)$(man3dir)
MANPAGES_DESTDIR_1 = $(DESTDIR)$(man1dir)

DOCS_DESTDIR = $(DESTDIR)$(docdir)

all: md2man $(TXTFILES) | $(MANPAGES_BUILDDIR)

$(MANPAGES_BUILDDIR) $(MANPAGES_WEBDIR_LINUX) $(MANPAGES_WEBDIR_WINDOWS):
	$(MKDIR) -p $@

%.txt: %
	man ./$< > $@

groff: $(MANPAGES_7_GROFF) $(MANPAGES_5_GROFF) $(MANPAGES_3_GROFF) $(MANPAGES_1_GROFF)

html: $(HTMLFILES)

%.html: %
	groff -mandoc -Thtml ./$< > $@

md2man:
	$(foreach f, $(MANPAGES_7_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_5_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_3_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)
	$(foreach f, $(MANPAGES_1_MD), ../utils/md2man.sh $(f) default.man $(MANPAGES_BUILDDIR)/$(basename $(notdir $(f)));)

web: | $(MANPAGES_WEBDIR_LINUX) $(MANPAGES_WEBDIR_WINDOWS)
	$(MAKE) -C generated all
	$(foreach f, $(MANPAGES_7_MD), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_5_MD), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_3_MD), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_1_MD), WEB=1 WIN32="" ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_LINUX)/$(f);)
	$(foreach f, $(MANPAGES_7_MD), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_5_MD), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_3_MD), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)
	$(foreach f, $(MANPAGES_1_MD), WEB=1 WIN32=1 ../utils/md2man.sh $(f) default.man $(MANPAGES_WEBDIR_WINDOWS)/$(f);)

compress: $(GZFILES_BUILD)

%.gz:
	gzip -c ./$* > $@

clean:

clobber: clean
	$(RM) -rf $(MANPAGES_BUILDDIR)/*.txt \
		$(MANPAGES_BUILDDIR)/*.html \
		$(MANPAGES_BUILDDIR)/*.gz \
		$(MANPAGES_WEBDIR_LINUX) \
		$(MANPAGES_WEBDIR_WINDOWS)

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

.PHONY: all html clean compress clobber cstyle install uninstall
