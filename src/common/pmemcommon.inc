# Copyright 2017-2019, Intel Corporation
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
# src/pmemcommon.inc -- common SOURCE definitions for PMDK libraries
#

SOURCE =\
	$(COMMON)/alloc.c\
	$(COMMON)/badblock_$(OS_DIMM).c\
	$(COMMON)/badblock.c\
	$(COMMON)/ctl.c\
	$(COMMON)/ctl_prefault.c\
	$(COMMON)/ctl_sds.c\
	$(COMMON)/ctl_fallocate.c\
	$(COMMON)/ctl_cow.c\
	$(call osdep, $(COMMON)/extent,.c)\
	$(COMMON)/file.c\
	$(COMMON)/file_posix.c\
	$(COMMON)/fs_posix.c\
	$(COMMON)/mmap.c\
	$(COMMON)/mmap_posix.c\
	$(COMMON)/os_posix.c\
	$(COMMON)/os_thread_posix.c\
	$(COMMON)/os_dimm_$(OS_DIMM).c\
	$(COMMON)/os_deep_linux.c\
	$(COMMON)/out.c\
	$(COMMON)/pool_hdr.c\
	$(COMMON)/set.c\
	$(COMMON)/shutdown_state.c\
	$(COMMON)/util.c\
	$(COMMON)/util_posix.c\
	$(COMMON)/uuid.c\
	$(call osdep, $(COMMON)/uuid,.c)

ifeq ($(OS_KERNEL_NAME),Linux)
SOURCE += $(COMMON)/os_auto_flush_linux.c
else
SOURCE += $(COMMON)/os_auto_flush_none.c
endif
