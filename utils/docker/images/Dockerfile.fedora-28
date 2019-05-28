#
# Copyright 2016-2018, Intel Corporation
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
# Dockerfile - a 'recipe' for Docker to build an image of fedora-based
#              environment for building the PMDK project.
#

# Pull base image
FROM fedora:28
MAINTAINER marcin.slusarz@intel.com

# Install basic tools
RUN dnf update -y

RUN dnf install -y \
	asciidoc \
	asciidoctor \
	autoconf \
	automake \
	bash-completion \
	bc \
	clang \
	file \
	findutils \
	fuse \
	fuse-devel \
	gcc \
	gdb \
	git \
	glib2-devel \
	hub \
	json-c-devel \
	keyutils-libs-devel \
	kmod-devel \
	lbzip2 \
	libtool \
	libudev-devel \
	libunwind-devel \
	libuuid-devel \
	libuv-devel \
	make \
	man \
	ncurses-devel \
	openssh-server \
	pandoc \
	passwd \
	pkgconfig \
	rpm-build \
	rpm-build-libs \
	rpmdevtools \
	rsync \
	sudo \
	tar \
	wget \
	which \
	xmlto

RUN dnf clean all

# Install libndctl
COPY install-libndctl.sh install-libndctl.sh
RUN ./install-libndctl.sh tags/v64.1 fedora

# Install valgrind
COPY install-valgrind.sh install-valgrind.sh
RUN ./install-valgrind.sh

# Install libfabric
COPY install-libfabric.sh install-libfabric.sh
RUN ./install-libfabric.sh fedora

# Add user
ENV USER pmdkuser
ENV USERPASS pmdkpass
RUN useradd -m $USER
RUN echo $USERPASS | passwd $USER --stdin
RUN gpasswd wheel -a $USER
USER $USER

# Set required environment variables
ENV OS fedora
ENV OS_VER 28
ENV START_SSH_COMMAND /usr/sbin/sshd
ENV PACKAGE_MANAGER rpm
ENV NOTTY 1

