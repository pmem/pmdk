#!/usr/bin/env bash
#
# Copyright 2014-2017, Intel Corporation
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

# two arguments - BUILD_RPMEM and DISTRO are defined
# and used only for build-rpm and they are ignored here
if [ $# -lt 6 -o $# -gt 9 ]
then
	echo "Usage: $(basename $0) VERSION_TAG SOURCE_DIR WORKING_DIR"\
					"OUT_DIR EXPERIMENTAL RUN_CHECK"\
					"BUILD_RPMEM [TEST_CONFIG_FILE] [NDCTL_DISABLE] [DISTRO] "
	exit 1
fi

PACKAGE_VERSION_TAG=$1
SOURCE=$2
WORKING_DIR=$3
OUT_DIR=$4
EXPERIMENTAL=$5
BUILD_PACKAGE_CHECK=$6
TEST_CONFIG_FILE=$8
NDCTL_DISABLE=$9

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
LIBFABRIC_MIN_VERSION=1.4.2

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

function rpmem_install_triggers_overrides() {
cat << EOF > debian/librpmem.install
$LIB_DIR/librpmem.so.*
EOF

cat << EOF > debian/librpmem.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
librpmem: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/librpmem-dev.install
$LIB_DIR/pmdk_debug/librpmem.a $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/librpmem.so $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/librpmem.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/librpmem.so
$LIB_DIR/pkgconfig/librpmem.pc
$INC_DIR/librpmem.h
$MAN7_DIR/librpmem.7.gz
$MAN3_DIR/rpmem_*.3.gz
EOF

cat << EOF > debian/librpmem-dev.triggers
interest man-db
EOF

cat << EOF > debian/librpmem-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
EOF

cat << EOF > debian/rpmemd.install
usr/bin/rpmemd
$MAN1_DIR/rpmemd.1.gz
EOF

cat << EOF > debian/rpmemd.triggers
interest man-db
EOF

cat << EOF > debian/rpmemd.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF
}

function append_rpmem_control() {
cat << EOF >> $CONTROL_FILE

Package: librpmem
Architecture: any
Depends: libfabric (>= $LIBFABRIC_MIN_VERSION), \${shlibs:Depends}, \${misc:Depends}
Description: PMDK librpmem library
 PMDK Library for Remote Persistent Memory support

Package: librpmem-dev
Section: libdevel
Architecture: any
Depends: librpmem (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for librpmem
 Development files for librpmem library.

Package: rpmemd
Section: misc
Architecture: any
Priority: optional
Depends: libfabric (>= $LIBFABRIC_MIN_VERSION), \${shlibs:Depends}, \${misc:Depends}
Description: rpmem daemon
 Daemon for Remote Persistent Memory support
EOF
}

if [ "${BUILD_PACKAGE_CHECK}" == "y" ]
then
CHECK_CMD="
override_dh_auto_test:
	dh_auto_test
	if [ -f $TEST_CONFIG_FILE ]; then\
		cp $TEST_CONFIG_FILE src/test/testconfig.sh;\
	else\
	        cp src/test/testconfig.sh.example src/test/testconfig.sh;\
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
MAGIC_INSTALL=utils/magic-install.sh
MAGIC_UNINSTALL=utils/magic-uninstall.sh
CONTROL_FILE=debian/control
OBJ_CPP_NAME=libpmemobj++-dev
OBJ_CPP_DOC_DIR=${OBJ_CPP_NAME}-${PACKAGE_VERSION}

[ -d $WORKING_DIR ] || mkdir $WORKING_DIR
[ -d $OUT_DIR ] || mkdir $OUT_DIR

OLD_DIR=$PWD

cd $WORKING_DIR

check_dir $SOURCE

mv $SOURCE $PACKAGE_SOURCE
tar zcf $PACKAGE_TARBALL_ORIG $PACKAGE_SOURCE

cd $PACKAGE_SOURCE

mkdir debian

# Generate compat file
cat << EOF > debian/compat
9
EOF

# Generate control file
cat << EOF > $CONTROL_FILE
Source: $PACKAGE_NAME
Maintainer: $PACKAGE_MAINTAINER
Section: misc
Priority: optional
Standards-version: 3.9.4
Build-Depends: debhelper (>= 9), doxygen

Package: libpmem
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: PMDK libpmem library
 PMDK Library for Persistent Memory support

Package: libpmem-dev
Section: libdevel
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmem
 Development files for libpmem library.

Package: libpmemblk
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: PMDK libpmemblk library
 PMDK Library for Persistent Memory support - block memory pool.

Package: libpmemblk-dev
Section: libdevel
Architecture: any
Depends: libpmemblk (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemblk
 Development files for libpmemblk library.

Package: libpmemlog
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: PMDK libpmemlog library
 PMDK Library for Persistent Memory support - log memory pool.

Package: libpmemlog-dev
Section: libdevel
Architecture: any
Depends: libpmemlog (=\${binary:Version}), libpmem-dev,  \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemlog
 Development files for libpmemlog library.

Package: libpmemcto
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: PMDK libpmemcto library
 PMDK Library for Persistent Memory support - close-to-open pesistence.

Package: libpmemcto-dev
Section: libdevel
Architecture: any
Depends: libpmemcto (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemcto
 Development files for libpmemcto library.

Package: libpmemobj
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: PMDK libpmemobj library
 PMDK Library for Persistent Memory support - transactional object store.

Package: libpmemobj-dev
Section: libdevel
Architecture: any
Depends: libpmemobj (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemobj
 Development files for libpmemobj library.

Package: libpmempool
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: PMDK libpmempool library
 PMDK Library for Persistent Memory support - pool management library.

Package: libpmempool-dev
Section: libdevel
Architecture: any
Depends: libpmempool (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmempool
 Development files for libpmempool library.

Package: libvmem
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: PMDK libvmem library
 PMDK Library for volatile-memory support on top of Persistent Memory.

Package: libvmem-dev
Section: libdevel
Architecture: any
Depends: libvmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libvmem
 Development files for libvmem library.

Package: libvmmalloc
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: PMDK libvmmalloc library
 PMDK general purpose volatile-memory allocation library on top of Persistent Memory.

Package: libvmmalloc-dev
Section: libdevel
Architecture: any
Depends: libvmmalloc (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libvmmalloc
 Development files for libvmmalloc library.

Package: $PACKAGE_NAME-dbg
Section: debug
Priority: extra
Architecture: any
Depends: libvmem (=\${binary:Version}), libvmmalloc (=\${binary:Version}), libpmem (=\${binary:Version}), libpmemblk (=\${binary:Version}), libpmemlog (=\${binary:Version}), libpmemobj (=\${binary:Version}), libpmemcto (=\${binary:Version}), libpmempool (=\${binary:Version}), \${misc:Depends}
Description: Debug symbols for PMDK libraries
 Debug symbols for all PMDK libraries.

Package: $PACKAGE_NAME-tools
Section: misc
Architecture: any
Priority: optional
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Tools for $PACKAGE_NAME
 Utilities for $PACKAGE_NAME.

Package: ${OBJ_CPP_NAME}
Section: libdevel
Architecture: any
Depends: libpmemobj-dev (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: C++ bindings for libpmemobj
 Headers-only C++ library for libpmemobj.
EOF

cp LICENSE debian/copyright

cat << EOF > debian/rules
#!/usr/bin/make -f
#export DH_VERBOSE=1
%:
	dh \$@

override_dh_strip:
	dh_strip --dbg-package=$PACKAGE_NAME-dbg

override_dh_auto_install:
	dh_auto_install -- EXPERIMENTAL=${EXPERIMENTAL} CPP_DOC_DIR="${OBJ_CPP_DOC_DIR}" prefix=/$PREFIX libdir=/$LIB_DIR includedir=/$INC_DIR docdir=/$DOC_DIR man1dir=/$MAN1_DIR man3dir=/$MAN3_DIR man5dir=/$MAN5_DIR man7dir=/$MAN7_DIR sysconfdir=/etc NORPATH=1

override_dh_install:
	mkdir -p debian/tmp/usr/share/pmdk/
	cp utils/pmdk.magic debian/tmp/usr/share/pmdk/
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

cat << EOF > debian/libpmem.install
$LIB_DIR/libpmem.so.*
usr/share/pmdk/pmdk.magic
EOF

cat $MAGIC_INSTALL > debian/libpmem.postinst
cat $MAGIC_UNINSTALL > debian/libpmem.prerm

cat << EOF > debian/libpmem.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmem: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmem-dev.install
$LIB_DIR/pmdk_debug/libpmem.a $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmem.so	$LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmem.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/libpmem.so
$LIB_DIR/pkgconfig/libpmem.pc
$INC_DIR/libpmem.h
$MAN7_DIR/libpmem.7.gz
$MAN3_DIR/pmem_*.3.gz
EOF

cat << EOF > debian/libpmem-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmem-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
EOF

cat << EOF > debian/libpmemblk.install
$LIB_DIR/libpmemblk.so.*
EOF

cat << EOF > debian/libpmemblk.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmemblk: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmemblk-dev.install
$LIB_DIR/pmdk_debug/libpmemblk.a $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmemblk.so $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmemblk.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/libpmemblk.so
$LIB_DIR/pkgconfig/libpmemblk.pc
$INC_DIR/libpmemblk.h
$MAN7_DIR/libpmemblk.7.gz
$MAN5_DIR/poolset.5.gz
$MAN3_DIR/pmemblk_*.3.gz
EOF

cat << EOF > debian/libpmemblk-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmemblk-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
EOF

cat << EOF > debian/libpmemlog.install
$LIB_DIR/libpmemlog.so.*
EOF

cat << EOF > debian/libpmemlog.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmemlog: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmemlog-dev.install
$LIB_DIR/pmdk_debug/libpmemlog.a $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmemlog.so $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmemlog.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/libpmemlog.so
$LIB_DIR/pkgconfig/libpmemlog.pc
$INC_DIR/libpmemlog.h
$MAN7_DIR/libpmemlog.7.gz
$MAN5_DIR/poolset.5.gz
$MAN3_DIR/pmemlog_*.3.gz
EOF

cat << EOF > debian/libpmemlog-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmemlog-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
EOF

cat << EOF > debian/libpmemcto.install
$LIB_DIR/libpmemcto.so.*
EOF

cat << EOF > debian/libpmemcto.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmemcto: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmemcto-dev.install
$LIB_DIR/pmdk_debug/libpmemcto.a $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmemcto.so $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmemcto.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/libpmemcto.so
$LIB_DIR/pkgconfig/libpmemcto.pc
$INC_DIR/libpmemcto.h
$MAN7_DIR/libpmemcto.7.gz
$MAN5_DIR/poolset.5.gz
$MAN3_DIR/pmemcto*.3.gz
EOF

cat << EOF > debian/libpmemcto-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmemcto-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
EOF

cat << EOF > debian/libpmemobj.install
$LIB_DIR/libpmemobj.so.*
EOF

cat << EOF > debian/libpmemobj.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmemobj: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmemobj-dev.install
$LIB_DIR/pmdk_debug/libpmemobj.a $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmemobj.so $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmemobj.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/libpmemobj.so
$LIB_DIR/pkgconfig/libpmemobj.pc
$INC_DIR/libpmemobj.h
$INC_DIR/libpmemobj/*.h
$MAN7_DIR/libpmemobj.7.gz
$MAN5_DIR/poolset.5.gz
$MAN3_DIR/pmemobj_*.3.gz
$MAN3_DIR/pobj_*.3.gz
$MAN3_DIR/oid_*.3.gz
$MAN3_DIR/toid*.3.gz
$MAN3_DIR/direct_*.3.gz
$MAN3_DIR/d_r*.3.gz
$MAN3_DIR/tx_*.3.gz
EOF

cat << EOF > debian/libpmemobj-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmemobj-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
EOF

cat << EOF > debian/libpmempool.install
$LIB_DIR/libpmempool.so.*
EOF

cat << EOF > debian/libpmempool.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmempool: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmempool-dev.install
$LIB_DIR/pmdk_debug/libpmempool.a $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmempool.so $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libpmempool.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/libpmempool.so
$LIB_DIR/pkgconfig/libpmempool.pc
$INC_DIR/libpmempool.h
$MAN7_DIR/libpmempool.7.gz
$MAN5_DIR/poolset.5.gz
$MAN3_DIR/pmempool_*.3.gz
EOF

cat << EOF > debian/libpmempool-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmempool-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
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
$LIB_DIR/pmdk_debug/libvmem.a $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libvmem.so	$LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libvmem.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/libvmem.so
$LIB_DIR/pkgconfig/libvmem.pc
$INC_DIR/libvmem.h
$MAN7_DIR/libvmem.7.gz
$MAN3_DIR/vmem_*.3.gz
EOF

cat << EOF > debian/libvmem-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
EOF

cat << EOF > debian/libvmem-dev.triggers
interest man-db
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
$LIB_DIR/pmdk_debug/libvmmalloc.a   $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libvmmalloc.so   $LIB_DIR/pmdk_dbg/
$LIB_DIR/pmdk_debug/libvmmalloc.so.* $LIB_DIR/pmdk_dbg/
$LIB_DIR/libvmmalloc.so
$LIB_DIR/pkgconfig/libvmmalloc.pc
$INC_DIR/libvmmalloc.h
$MAN7_DIR/libvmmalloc.7.gz
EOF

cat << EOF > debian/libvmmalloc-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions $LIB_DIR/pmdk_dbg/*
EOF

cat << EOF > debian/libvmmalloc-dev.triggers
interest man-db
EOF

cat << EOF > debian/$PACKAGE_NAME-dbg.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF

cat << EOF > debian/$PACKAGE_NAME-tools.install
usr/bin/pmempool
$MAN1_DIR/pmempool.1.gz
$MAN1_DIR/pmempool-*.1.gz
etc/bash_completion.d/pmempool.sh
EOF

cat << EOF > debian/$PACKAGE_NAME-tools.triggers
interest man-db
EOF

cat << EOF > debian/$PACKAGE_NAME-tools.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF

cat << EOF > debian/${OBJ_CPP_NAME}.install
$INC_DIR/libpmemobj++/*.hpp
$INC_DIR/libpmemobj++/detail/*.hpp
$DOC_DIR/${OBJ_CPP_DOC_DIR}/*
$LIB_DIR/pkgconfig/libpmemobj++.pc
EOF

cat << EOF > debian/${OBJ_CPP_NAME}.triggers
interest doc-base
EOF

cat << EOF > debian/${OBJ_CPP_NAME}.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
EOF

cat << EOF > debian/${OBJ_CPP_NAME}.doc-base
Document: ${OBJ_CPP_NAME}
Title: PMDK libpmemobj C++ bindings Manual
Author: PMDK Developers
Abstract: This is the HTML docs for the C++ bindings for PMDK's libpmemobj.
Section: Programming

Format: HTML
Index: /$DOC_DIR/${OBJ_CPP_DOC_DIR}/index.html
Files: /$DOC_DIR/${OBJ_CPP_DOC_DIR}/*
EOF

# librpmem & rpmemd
if [ "${BUILD_RPMEM}" = "y" -a "${RPMEM_DPKG}" = "y" ]
then
	append_rpmem_control;
	rpmem_install_triggers_overrides;
fi

if [ "$NDCTL_DISABLE" == "y" ]
then
	export EXTRA_CFLAGS+=" -DNDCTL_DISABLE "
fi

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
