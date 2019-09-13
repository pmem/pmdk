#!/usr/bin/env bash
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
# build-rpm.sh - Script for building rpm packages
#

set -e

SCRIPT_DIR=$(dirname $0)
source $SCRIPT_DIR/pkg-common.sh

check_tool rpmbuild
check_file $SCRIPT_DIR/pkg-config.sh
source $SCRIPT_DIR/pkg-config.sh

#
# usage -- print usage message and exit
#
usage()
{
	[ "$1" ] && echo Error: $1
	cat >&2 <<EOF
Usage: $0 [ -h ] -t version-tag -s source-dir -w working-dir -o output-dir
	[ -d distro ] [ -e build-experimental ] [ -c run-check ]
	[ -f testconfig-file ]

-h			print this help message
-t version-tag		source version tag
-s source-dir		source directory
-w working-dir		working directory
-o output-dir		outut directory
-d distro		Linux distro name
-e build-experimental	build experimental packages
-c run-check		run package check
-f testconfig-file	custom testconfig.sh
EOF
	exit 1
}

#
# command-line argument processing...
#
args=`getopt he:c:r:n:t:d:s:w:o:f: $*`
[ $? != 0 ] && usage
set -- $args
for arg
do
	receivetype=auto
	case "$arg"
	in
	-e)
		EXPERIMENTAL="$2"
		shift 2
		;;
	-c)
		BUILD_PACKAGE_CHECK="$2"
		shift 2
		;;
	-f)
		TEST_CONFIG_FILE="$2"
		shift 2
		;;
	-t)
		PACKAGE_VERSION_TAG="$2"
		shift 2
		;;
	-s)
		SOURCE="$2"
		shift 2
		;;
	-w)
		WORKING_DIR="$2"
		shift 2
		;;
	-o)
		OUT_DIR="$2"
		shift 2
		;;
	-d)
		DISTRO="$2"
		shift 2
		;;
	--)
		shift
		break
		;;
	esac
done


# check for mandatory arguments
if [ -z "$PACKAGE_VERSION_TAG" -o -z "$SOURCE" -o -z "$WORKING_DIR" -o -z "$OUT_DIR" ]
then
	error "Mandatory arguments missing"
	usage
fi

# detected distro or defined in cmd
if [ -z "${DISTRO}" ]
then
	OS=$(get_os)
	if [ "$OS" != "1" ]
	then
		echo "Detected OS: $OS"
		DISTRO=$OS
	else
		error "Unknown distribution"
		exit 1
	fi
fi


if [ "$EXTRA_CFLAGS_RELEASE" = "" ]; then
	export EXTRA_CFLAGS_RELEASE="-ggdb -fno-omit-frame-pointer"
fi

RPMBUILD_OPTS=( )
PACKAGE_VERSION=$(get_version $PACKAGE_VERSION_TAG)

if [ -z "$PACKAGE_VERSION" ]
then
	error "Can not parse version from '${PACKAGE_VERSION_TAG}'"
	exit 1
fi


PACKAGE_SOURCE=${PACKAGE_NAME}-${PACKAGE_VERSION}
SOURCE=$PACKAGE_NAME
PACKAGE_TARBALL=$PACKAGE_SOURCE.tar.gz
RPM_SPEC_FILE=$PACKAGE_SOURCE/$PACKAGE_NAME.spec
OLDPWD=$PWD

[ -d $WORKING_DIR ] || mkdir -v $WORKING_DIR
[ -d $OUT_DIR ] || mkdir $OUT_DIR

cd $WORKING_DIR

check_dir $SOURCE
mv $SOURCE $PACKAGE_SOURCE

if [ "$DISTRO" = "SLES_like" ]
then
	RPM_LICENSE="BSD-3-Clause"
	RPM_GROUP_SYS_BASE="System\/Base"
	RPM_GROUP_SYS_LIBS="System\/Libraries"
	RPM_GROUP_DEV_LIBS="Development\/Libraries\/C and C++"
	RPM_PKG_NAME_SUFFIX="1"
	RPM_MAKE_FLAGS="BINDIR=""%_bindir"" NORPATH=1"
	RPM_MAKE_INSTALL="%fdupes %{buildroot}\/%{_prefix}"
else
	RPM_LICENSE="BSD"
	RPM_GROUP_SYS_BASE="System Environment\/Base"
	RPM_GROUP_SYS_LIBS="System Environment\/Libraries"
	RPM_GROUP_DEV_LIBS="Development\/Libraries"
	RPM_PKG_NAME_SUFFIX=""
	RPM_MAKE_FLAGS="NORPATH=1"
	RPM_MAKE_INSTALL=""
fi

#
# Create parametrized spec file required by rpmbuild.
# Most of variables are set in pkg-config.sh file in order to
# keep descriptive values separately from this script.
#
sed -e "s/__VERSION__/$PACKAGE_VERSION/g" \
	-e "s/__LICENSE__/$RPM_LICENSE/g" \
	-e "s/__PACKAGE_MAINTAINER__/$PACKAGE_MAINTAINER/g" \
	-e "s/__PACKAGE_SUMMARY__/$PACKAGE_SUMMARY/g" \
	-e "s/__GROUP_SYS_BASE__/$RPM_GROUP_SYS_BASE/g" \
	-e "s/__GROUP_SYS_LIBS__/$RPM_GROUP_SYS_LIBS/g" \
	-e "s/__GROUP_DEV_LIBS__/$RPM_GROUP_DEV_LIBS/g" \
	-e "s/__PKG_NAME_SUFFIX__/$RPM_PKG_NAME_SUFFIX/g" \
	-e "s/__MAKE_FLAGS__/$RPM_MAKE_FLAGS/g" \
	-e "s/__MAKE_INSTALL_FDUPES__/$RPM_MAKE_INSTALL/g" \
	-e "s/__LIBFABRIC_MIN_VER__/$LIBFABRIC_MIN_VERSION/g" \
	-e "s/__NDCTL_MIN_VER__/$NDCTL_MIN_VERSION/g" \
	$OLDPWD/$SCRIPT_DIR/vmem.spec.in > $RPM_SPEC_FILE

if [ "$DISTRO" = "SLES_like" ]
then
	sed -i '/^#.*bugzilla.redhat/d' $RPM_SPEC_FILE
fi

# do not split on space
IFS=$'\n'

# experimental features
if [ "${EXPERIMENTAL}" = "y" ]
then
	# no experimental features for now
	RPMBUILD_OPTS+=( )
fi

# use specified testconfig file or default
if [[( -n "${TEST_CONFIG_FILE}") && ( -f "$TEST_CONFIG_FILE" ) ]]
then
	echo "Test config file: $TEST_CONFIG_FILE"
	RPMBUILD_OPTS+=(--define "_testconfig $TEST_CONFIG_FILE")
else
	echo -e "Test config file $TEST_CONFIG_FILE does not exist.\n"\
		"Default test config will be used."
fi

# run make check or not
if [ "${BUILD_PACKAGE_CHECK}" == "n" ]
then
	RPMBUILD_OPTS+=(--define "_skip_check 1")
fi

tar zcf $PACKAGE_TARBALL $PACKAGE_SOURCE

# Create directory structure for rpmbuild
mkdir -v BUILD SPECS

echo "opts: ${RPMBUILD_OPTS[@]}"

rpmbuild --define "_topdir `pwd`"\
	--define "_rpmdir ${OUT_DIR}"\
	--define "_srcrpmdir ${OUT_DIR}"\
	 -ta $PACKAGE_TARBALL \
	 ${RPMBUILD_OPTS[@]}

echo "Building rpm packages done"

exit 0
