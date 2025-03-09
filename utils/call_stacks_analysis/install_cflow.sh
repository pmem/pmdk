#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025, Hewlett Packard Enterprise Development LP
#
#
# Install cflow tool
#

function main() {
	if  which "cflow" >/dev/null 2>&1; then
		echo "cflow already installed"
		return 0
	fi
	wget "https://ftp.gnu.org/gnu/cflow/cflow-latest.tar.gz"
	tar -xvzf cflow-latest.tar.gz
	rm cflow-latest.tar.gz
	pushd cflow-*
	./configure && make && sudo make install
	popd
}

main
