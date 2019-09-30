#
# Copyright 2016-2017, Intel Corporation
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
#              environment for building the PMDK project.
#

# Pull base image
FROM ubuntu:18.04
MAINTAINER marcin.slusarz@intel.com
# Update the Apt cache and install basic tools
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
	asciidoc \
	autoconf \
	bc \
	build-essential \
	clang \
	debhelper \
	devscripts \
	flake8 \
	gcc \
	g++ \
	gdb \
	git \
	libc6-dbg \
	libfuse-dev \
	libglib2.0-dev \
	libjson-c-dev \
	libkmod-dev \
	libncurses5-dev \
	libtool \
	libudev-dev \
	libunwind-dev \
	libuv1-dev \
	pandoc \
	pkg-config \
	ruby \
	ssh \
	strace \
	sudo \
	systemd \
	unzip \
	uuid-dev \
	wget \
	whois \
	xmlto

RUN apt-get clean

RUN rm -rf /var/lib/apt/lists/*

# Install valgrind
COPY install-valgrind.sh install-valgrind.sh
RUN ./install-valgrind.sh

# Install libfabric
COPY install-libfabric.sh install-libfabric.sh
RUN ./install-libfabric.sh

# Install libndctl
COPY install-libndctl.sh install-libndctl.sh
RUN ./install-libndctl.sh tags/v60.1

# Add user
ENV USER pmdkuser
ENV USERPASS pmdkpass
RUN useradd -m $USER -g sudo -p `mkpasswd $USERPASS`
USER $USER

# Set required environment variables
ENV OS ubuntu
ENV OS_VER 18.04
ENV START_SSH_COMMAND service ssh start
ENV PACKAGE_MANAGER dpkg
ENV NOTTY 1
