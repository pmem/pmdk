#
# Copyright 2017, Intel Corporation
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
# src/version.inc -- common version rules for NVML makefiles
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))..

VERSION = $(TOP)/.version
GIT_VERSION = $(TOP)/.git_version

GIT_VERSION_TAG=$(shell cat $(TOP)/GIT_VERSION | grep tag: | sed 's/.*tag: \([0-9a-z.+-]*\).*/\1/')
GIT_VERSION_HASH=$(shell cat $(TOP)/GIT_VERSION | sed -e 's/.Format:%h %d.//g' -e 's/ .*//')

ifneq ($(GIT_VERSION_TAG),)
$(shell echo "$(GIT_VERSION_TAG)" > $(GIT_VERSION))
else
ifneq ($(GIT_VERSION_HASH),)
$(shell echo "$(GIT_VERSION_HASH)" > $(GIT_VERSION))
else
$(shell rm -f $(GIT_VERSION))
endif
endif

export SRCVERSION = $(shell git describe 2>/dev/null ||\
			cat $(VERSION) 2>/dev/null ||\
			git log -1 --format=%h 2>/dev/null ||\
			cat $(GIT_VERSION) 2>/dev/null ||\
			(basename `realpath $(TOP)` | sed 's/pmdk[-]*\([0-9a-z.+-]*\).*/\1/'))

ifeq ($(SRCVERSION),)
$(error Cannot evaluate version)
endif
