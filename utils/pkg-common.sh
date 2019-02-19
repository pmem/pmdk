#
# Copyright 2014-2019, Intel Corporation
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
