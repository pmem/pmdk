# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# pkg-common.sh - common functions and variables for building packages
#

export LC_ALL="C"

function error() {
	echo -e "error: $@"
}

function check_dir() {
	if [ ! -d $1 ]
	then
		error "Directory '$1' does not exist."
		exit 1
	fi
}

function check_file() {
	if [ ! -f $1 ]
	then
		error "File '$1' does not exist."
		exit 1
	fi
}

function check_tool() {
	local tool=$1
	if [ -z "$(which $tool 2>/dev/null)" ]
	then
		error "'${tool}' not installed or not in PATH"
		exit 1
	fi
}

function get_version() {
	echo -n $1 | sed "s/-rc/~rc/"
}

function get_os() {
	if [ -f /etc/os-release ]
	then
		local OS=$(cat /etc/os-release | grep -m1 -o -P '(?<=NAME=).*($)')
		[[ "$OS" =~ SLES|openSUSE ]] && echo -n "SLES_like" ||
		([[ "$OS" =~ "Fedora"|"Red Hat"|"CentOS" ]] && echo -n "RHEL_like" || echo 1)
	else
		echo 1
	fi
}

REGEX_DATE_AUTHOR="([a-zA-Z]{3} [a-zA-Z]{3} [0-9]{2} [0-9]{4})\s*(.*)"
REGEX_MESSAGE_START="\s*\*\s*(.*)"
REGEX_MESSAGE="\s*(\S.*)"
