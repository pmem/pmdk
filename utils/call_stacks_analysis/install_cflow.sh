#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025, Hewlett Packard Enterprise Development LP
#
#
# Install the cflow tool
#

if  which cflow >/dev/null 2>&1; then
	echo "cflow already installed"
	exit 0
fi

wget -q --show-progress "ftp://ftp.gnu.org/gnu/cflow/cflow-latest.tar.gz"
if [ $? != 0 ]; then
	echo "Can not download the cflow source code"
	exit 1
fi

tar -xzf cflow-latest.tar.gz && ls cflow-* > /dev/null 2>&1
status=$?
rm cflow-latest.tar.gz
if [ $status != 0 ]; then
	echo "Unexpected contents of the source code package"
	exit 1
fi

cd cflow-*

./configure && make && sudo make install
status=$?

cd - >/dev/null
rm -rf cflow-*

if [ $status != 0 ]; then
	echo "Can not install cflow"
fi

exit $status
