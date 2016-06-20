#
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
# Dockerfile - a 'recipe' for Docker to build an image of ubuntu-based
#              environment for building the NVML project.
#

# Pull base image
FROM ubuntu:16.04
MAINTAINER wojciech.uss@intel.com

ENV NOTTY=1

# Update the Apt cache and install basic tools
RUN apt-get update
RUN apt-get install -y software-properties-common libunwind8-dev autoconf \
	devscripts pkg-config ssh git gcc clang debhelper vim sudo whois \
	libc6-dbg libncurses5-dev libuv1-dev fuse libglib2.0-dev 

# Install valgrind
RUN git clone https://github.com/wojtuss/valgrind.git \
	&& cd valgrind \
	&& ./autogen.sh \
	&& ./configure \
	&& make \
	&& make install
RUN rm -rf valgrind

# Install libfabric
ENV libfabric_ver 1.2.0
ENV libfabric_url https://github.com/ofiwg/libfabric/releases/download
ENV libfabric_dir libfabric-$libfabric_ver
ENV libfabric_tarball ${libfabric_dir}.tar.bz2
RUN wget "${libfabric_url}/v${libfabric_ver}/${libfabric_tarball}"
RUN tar -xf $libfabric_tarball
RUN cd $libfabric_dir \
	&& ./configure --prefix=/usr --enable-sockets \
	&& make -j2 \
	&& make install
RUN rm -f ${libfabric_tarball}
RUN rm -rf ${libfabric_dir}

# Add user
ENV USER nvmluser
ENV USERPASS nvmlpass
RUN useradd -m $USER -g sudo -p `mkpasswd $USERPASS`

USER $USER
