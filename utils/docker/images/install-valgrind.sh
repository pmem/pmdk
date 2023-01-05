#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# install-valgrind.sh - installs valgrind for persistent memory
#

set -e

OS=$1

install_upstream_from_distro() {
  case "$OS" in
    fedora) dnf install -y valgrind ;;
    ubuntu) apt-get install -y --no-install-recommends valgrind ;;
    *) return 1 ;;
  esac
}

install_upstream_3_16_1() {
  git clone git://sourceware.org/git/valgrind.git
  cd valgrind
  # valgrind v3.16.1 upstream
  git checkout VALGRIND_3_16_BRANCH
  ./autogen.sh
  ./configure
  make -j$(nproc)
  make -j$(nproc) install
  cd ..
  rm -rf valgrind
}

install_custom-pmem_from_source() {
  git clone https://github.com/pmem/valgrind.git
  cd valgrind
  # valgrind v3.20 with pmemcheck
  git checkout b21a0ab76d2fbc4f26d2b7c7e20df63d63f0a31b
  ./autogen.sh
  ./configure
  make -j$(nproc)
  make -j$(nproc) install
  cd ..
  rm -rf valgrind
}

ARCH=$(uname -m)
case "$ARCH" in
  ppc64le) install_upstream_3_16_1 ;;
  aarch64) install_upstream_3_16_1 ;;
  *) install_custom-pmem_from_source ;;
esac
