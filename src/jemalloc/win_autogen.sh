#!/bin/sh
# Copyright 2016, Intel Corporation
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

JEMALLOC_GEN=./../windows/jemalloc_gen
AC_PATH=./../../jemalloc

autoconf
if [ $? -ne 0 ]; then
	echo "Error $? in $i"
	exit 1
fi

if [ ! -d "$JEMALLOC_GEN" ]; then
	echo Creating... $JEMALLOC_GEN
	mkdir "$JEMALLOC_GEN"
fi

cd $JEMALLOC_GEN

echo "Run configure..."
$AC_PATH/configure \
	--enable-autogen \
	CC=cl \
	--enable-lazy-lock=no \
	--without-export \
	--with-jemalloc-prefix=je_vmem_ \
	--with-private-namespace=je_vmem_ \
	--disable-xmalloc \
	--disable-munmap \
	EXTRA_CFLAGS="-DJEMALLOC_LIBVMEM"

if [ $? -ne 0 ]; then
    echo "Error $? in $AC_PATH/configure"
    exit 1
fi
