#
# Copyright 2017-2018, Intel Corporation
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

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../../../
include $(TOP)/src/common.inc

SFML := $(call check_package, sfml-all --atleast-version 2.4)
ifeq ($(SFML),y)
ifeq ($(shell uname -s),FreeBSD)
FONTDIR=/usr/local/share/fonts
else
FONTDIR=/usr/share/fonts
endif
PROGS = pmpong
else
$(info NOTE: Skipping pmpong because sfml is missing \
-- see src/examples/libpmemobj++/pmpong/README for details.)
endif

LIBS = -lpmemobj -lpmem

ifeq ($(SFML),y)
LIBS += $(shell $(PKG_CONFIG) --libs sfml-all)
endif

COMPILE_LANG = cpp
include ../../Makefile.inc
INCS += -I../

pmpong: MainGame.o Paddle.o Ball.o PongGameStatus.o GameController.o Pool.o GameView.o MenuView.o GameOverView.o

rm-fontconf:
	rm -f fontConf

clobber: rm-fontconf

clean: rm-fontconf

fontConf:
	find $(FONTDIR) -name *.ttf | head -n 1 > fontConf

all: fontConf
