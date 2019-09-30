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
# build-dpkg.sh - Script for building deb packages
#

set -e

SCRIPT_DIR=$(dirname $0)
source $SCRIPT_DIR/pkg-common.sh

#
# usage -- print usage message and exit
#
usage()
{
	[ "$1" ] && echo Error: $1
	cat >&2 <<EOF
Usage: $0 [ -h ] -t version-tag -s source-dir -w working-dir -o output-dir
	[ -e build-experimental ] [ -c run-check ] [ -f testconfig-file ]

-h			print this help message
-t version-tag		source version tag
-s source-dir		source directory
-w working-dir		working directory
-o output-dir		output directory
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


PREFIX=usr
LIB_DIR=$PREFIX/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)
INC_DIR=$PREFIX/include
MAN1_DIR=$PREFIX/share/man/man1
MAN3_DIR=$PREFIX/share/man/man3
MAN5_DIR=$PREFIX/share/man/man5
MAN7_DIR=$PREFIX/share/man/man7

DOC_DIR=$PREFIX/share/doc
if [ "$EXTRA_CFLAGS_RELEASE" = "" ]; then
	export EXTRA_CFLAGS_RELEASE="-ggdb -fno-omit-frame-pointer"
fi

function convert_changelog() {
	while read line
	do
		if [[ $line =~ $REGEX_DATE_AUTHOR ]]
		then
			DATE="${BASH_REMATCH[1]}"
			AUTHOR="${BASH_REMATCH[2]}"
			echo "  * ${DATE} ${AUTHOR}"
		elif [[ $line =~ $REGEX_MESSAGE_START ]]
		then
			MESSAGE="${BASH_REMATCH[1]}"
			echo "  - ${MESSAGE}"
		elif [[ $line =~ $REGEX_MESSAGE ]]
		then
			MESSAGE="${BASH_REMATCH[1]}"
			echo "    ${MESSAGE}"
		fi
	done < $1
}


if [ "${BUILD_PACKAGE_CHECK}" == "y" ]
then
CHECK_CMD="
override_dh_auto_test:
	dh_auto_test
	if [ -f $TEST_CONFIG_FILE ]; then\
		cp $TEST_CONFIG_FILE src/test/testconfig.sh;\
	else\
		echo 'PMEM_FS_DIR=/tmp' > src/test/testconfig.sh; \
		echo 'PMEM_FS_DIR_FORCE_PMEM=1' >> src/test/testconfig.sh; \
		echo 'TEST_BUILD=\"debug nondebug\"' >> src/test/testconfig.sh; \
		echo 'TEST_FS=\"pmem any none\"' >> src/test/testconfig.sh; \
	fi
	make pcheck ${PCHECK_OPTS}
"
else
CHECK_CMD="
override_dh_auto_test:

"
fi

check_tool debuild
check_tool dch
check_file $SCRIPT_DIR/pkg-config.sh

source $SCRIPT_DIR/pkg-config.sh

PACKAGE_VERSION=$(get_version $PACKAGE_VERSION_TAG)
PACKAGE_RELEASE=1
PACKAGE_SOURCE=${PACKAGE_NAME}-${PACKAGE_VERSION}
PACKAGE_TARBALL_ORIG=${PACKAGE_NAME}_${PACKAGE_VERSION}.orig.tar.gz
CONTROL_FILE=debian/control

[ -d $WORKING_DIR ] || mkdir $WORKING_DIR
[ -d $OUT_DIR ] || mkdir $OUT_DIR

OLD_DIR=$PWD

cd $WORKING_DIR

check_dir $SOURCE

mv $SOURCE $PACKAGE_SOURCE
tar zcf $PACKAGE_TARBALL_ORIG $PACKAGE_SOURCE

cd $PACKAGE_SOURCE

rm -rf debian
mkdir debian

# Generate compat file
cat << EOF > debian/compat
9
EOF

# Generate control file
cat << EOF > $CONTROL_FILE
Source: $PACKAGE_NAME
Maintainer: $PACKAGE_MAINTAINER
Section: libs
Priority: optional
Standards-version: 4.1.4
Build-Depends: debhelper (>= 9)
Homepage: http://pmem.io/pmdk/

Package: libvmem
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Persistent Memory volatile memory support library
 The libvmem library turns a pool of persistent memory into a volatile memory
 pool, similar to the system heap but kept separate and with its own
 malloc-style API.
 .
 libvmem supports the traditional malloc/free interfaces on a memory mapped
 file. This allows the use of persistent memory as volatile memory, for cases
 where the pool of persistent memory is useful to an application, but when the
 application doesn’t need it to be persistent.

Package: libvmem-dev
Section: libdevel
Architecture: any
Depends: libvmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libvmem
 The libvmem library turns a pool of persistent memory into a volatile memory
 pool, similar to the system heap but kept separate and with its own
 malloc-style API.
 .
 This package contains libraries and header files used for linking programs
 against libvmem.

Package: libvmmalloc
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Persistent Memory dynamic allocation support library
 The libvmmalloc library transparently converts all the dynamic memory
 allocations into persistent memory allocations. This allows the use of
 persistent memory as volatile memory without modifying the target
 application.

Package: libvmmalloc-dev
Section: libdevel
Architecture: any
Depends: libvmmalloc (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libvmmalloc
 The libvmmalloc library transparently converts all the dynamic memory
 allocations into persistent memory allocations.
 .
 This package contains libraries and header files used for linking programs
 against libvmalloc.

Package: $PACKAGE_NAME-dbg
Section: debug
Priority: optional
Architecture: any
Depends: libvmem (=\${binary:Version}), libvmmalloc (=\${binary:Version}), \${misc:Depends}
Description: Debug symbols for PMDK libraries
 Debug symbols for all PMDK libraries.

cp LICENSE debian/copyright

cat << EOF > debian/rules
#!/usr/bin/make -f
#export DH_VERBOSE=1
%:
	dh \$@

override_dh_strip:
	dh_strip --dbg-package=$PACKAGE_NAME-dbg

override_dh_auto_build:
	dh_auto_build -- EXPERIMENTAL=${EXPERIMENTAL} prefix=/$PREFIX libdir=/$LIB_DIR includedir=/$INC_DIR docdir=/$DOC_DIR man1dir=/$MAN1_DIR man3dir=/$MAN3_DIR man5dir=/$MAN5_DIR man7dir=/$MAN7_DIR SRCVERSION=$SRCVERSION

override_dh_auto_install:
	dh_auto_install -- EXPERIMENTAL=${EXPERIMENTAL} prefix=/$PREFIX libdir=/$LIB_DIR includedir=/$INC_DIR docdir=/$DOC_DIR man1dir=/$MAN1_DIR man3dir=/$MAN3_DIR man5dir=/$MAN5_DIR man7dir=/$MAN7_DIR SRCVERSION=$SRCVERSION
	find -path './debian/*usr/share/man/man*/*.gz' -exec gunzip {} \;

override_dh_install:
	mkdir -p debian/tmp/usr/share/vmem/
	dh_install

${CHECK_CMD}
EOF

chmod +x debian/rules

mkdir debian/source

ITP_BUG_EXCUSE="# This is our first package but we do not want to upload it yet.
# Please refer to Debian Developer's Reference section 5.1 (New packages) for details:
# https://www.debian.org/doc/manuals/developers-reference/pkgs.html#newpackage"

cat << EOF > debian/source/format
3.0 (quilt)
EOF

cat << EOF > debian/libvmem.install
$LIB_DIR/libvmem.so.*
EOF

cat << EOF > debian/libvmem.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libvmem: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libvmem-dev.install
$LIB_DIR/vmem_debug/libvmem.a $LIB_DIR/vmem_dbg/
$LIB_DIR/vmem_debug/libvmem.so	$LIB_DIR/vmem_dbg/
$LIB_DIR/vmem_debug/libvmem.so.* $LIB_DIR/vmem_dbg/
$LIB_DIR/libvmem.so
$LIB_DIR/pkgconfig/libvmem.pc
$INC_DIR/libvmem.h
$MAN7_DIR/libvmem.7
$MAN3_DIR/vmem_*.3
EOF

cat << EOF > debian/libvmem-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/vmem_dbg/*
# vmem provides second set of libraries for debugging.
# These are in /usr/lib/$arch/vmem_dbg/, but still trigger ldconfig.
# Related issue: https://github.com/pmem/issues/issues/841
libvmem-dev: package-has-unnecessary-activation-of-ldconfig-trigger
EOF

cat << EOF > debian/libvmmalloc.install
$LIB_DIR/libvmmalloc.so.*
EOF

cat << EOF > debian/libvmmalloc.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libvmmalloc: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libvmmalloc-dev.install
$LIB_DIR/vmem_debug/libvmmalloc.a   $LIB_DIR/vmem_dbg/
$LIB_DIR/vmem_debug/libvmmalloc.so   $LIB_DIR/vmem_dbg/
$LIB_DIR/vmem_debug/libvmmalloc.so.* $LIB_DIR/vmem_dbg/
$LIB_DIR/libvmmalloc.so
$LIB_DIR/pkgconfig/libvmmalloc.pc
$INC_DIR/libvmmalloc.h
$MAN7_DIR/libvmmalloc.7
EOF

cat << EOF > debian/libvmmalloc-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/vmem_dbg/*
# vmem provides second set of libraries for debugging.
# These are in /usr/lib/$arch/vmem_dbg/, but still trigger ldconfig.
# Related issue: https://github.com/pmem/issues/issues/841
libvmmalloc-dev: package-has-unnecessary-activation-of-ldconfig-trigger
EOF

cat << EOF > debian/$PACKAGE_NAME-dbg.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF

# Convert ChangeLog to debian format
CHANGELOG_TMP=changelog.tmp
dch --create --empty --package $PACKAGE_NAME -v $PACKAGE_VERSION-$PACKAGE_RELEASE -M -c $CHANGELOG_TMP
touch debian/changelog
head -n1 $CHANGELOG_TMP >> debian/changelog
echo "" >> debian/changelog
convert_changelog ChangeLog >> debian/changelog
echo "" >> debian/changelog
tail -n1 $CHANGELOG_TMP >> debian/changelog
rm $CHANGELOG_TMP

# This is our first release but we do
debuild --preserve-envvar=EXTRA_CFLAGS_RELEASE \
	--preserve-envvar=EXTRA_CFLAGS_DEBUG \
	--preserve-envvar=EXTRA_CFLAGS \
	--preserve-envvar=EXTRA_CXXFLAGS \
	--preserve-envvar=EXTRA_LDFLAGS \
	-us -uc

cd $OLD_DIR

find $WORKING_DIR -name "*.deb"\
		-or -name "*.dsc"\
		-or -name "*.changes"\
		-or -name "*.orig.tar.gz"\
		-or -name "*.debian.tar.gz" | while read FILE
do
	mv -v $FILE $OUT_DIR/
done

exit 0
