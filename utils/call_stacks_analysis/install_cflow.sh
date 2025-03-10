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

wget "https://mirror.easyname.at/gnu/cflow/cflow-latest.tar.gz"
if [ $? != 0 ]; then
	echo "Can not download the cflow source code"
	exit 1
fi

tar -xvzf cflow-latest.tar.gz
rm cflow-latest.tar.gz
cd cflow-*

./configure && make && sudo make install
status=$?

cd - >/dev/null
rm -rf cflow-*

if [ $status != 0 ]; then
	echo "Can not install the cflow"
fi

exit $status
