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
	[ -e build-experimental ] [ -c run-check ]
	[ -n with-ndctl ] [ -f testconfig-file ]

-h			print this help message
-t version-tag		source version tag
-s source-dir		source directory
-w working-dir		working directory
-o output-dir		output directory
-e build-experimental	build experimental packages
-c run-check		run package check
-n with-ndctl		build with libndctl
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
	-r)
		BUILD_RPMEM="$2"
		shift 2
		;;
	-n)
		NDCTL_ENABLE="$2"
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

LIBFABRIC_MIN_VERSION=1.4.2
NDCTL_MIN_VERSION=60.1

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
$MAN7_DIR/librpmem.7
$MAN3_DIR/rpmem_*.3
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
$MAN1_DIR/rpmemd.1
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
Description: Persistent Memory remote access support library
 librpmem provides low-level support for remote access to persistent memory
 (pmem) utilizing RDMA-capable RNICs. The library can be used to replicate
 remotely a memory region over RDMA protocol. It utilizes appropriate
 persistency mechanism based on remote node’s platform capabilities. The
 librpmem utilizes the ssh client to authenticate a user on remote node and for
 encryption of connection’s out-of-band configuration data.
 .
 This library is for applications that use remote persistent memory directly,
 without the help of any library-supplied transactions or memory allocation.
 Higher-level libraries that build on libpmem are available and are recommended
 for most applications.

Package: librpmem-dev
Section: libdevel
Architecture: any
Depends: librpmem (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for librpmem
 librpmem provides low-level support for remote access to persistent memory
 (pmem) utilizing RDMA-capable RNICs.
 .
 This package contains libraries and header files used for linking programs
 against librpmem.

Package: rpmemd
Section: misc
Architecture: any
Priority: optional
Depends: libfabric (>= $LIBFABRIC_MIN_VERSION), \${shlibs:Depends}, \${misc:Depends}
Description: rpmem daemon
 Daemon for Remote Persistent Memory support
EOF
}

function daxio_install_triggers_overrides() {
cat << EOF > debian/daxio.install
usr/bin/daxio
$MAN1_DIR/daxio.1
EOF

cat << EOF > debian/daxio.triggers
interest man-db
EOF

cat << EOF > debian/daxio.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF
}

function append_daxio_control() {
cat << EOF >> $CONTROL_FILE

Package: daxio
Section: misc
Architecture: any
Priority: optional
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: daxio utility
 The daxio utility performs I/O on Device DAX devices or zero
 a Device DAX device.  Since the standard I/O APIs (read/write) cannot be used
 with Device DAX, data transfer is performed on a memory-mapped device.
 The daxio may be used to dump Device DAX data to a file, restore data from
 a backup copy, move/copy data to another device or to erase data from
 a device.
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
MAGIC_INSTALL=utils/magic-install.sh
MAGIC_UNINSTALL=utils/magic-uninstall.sh
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

Package: libpmem2
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Persistent Memory low level support library
 libpmem2 provides low level persistent memory support. In particular, support
 for the persistent memory instructions for flushing changes to pmem is
 provided. (EXPERIMENTAL)

Package: libpmem2-dev
Section: libdevel
Architecture: any
Depends: libpmem2 (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmem2
 libpmem2 provides low level persistent memory support. In particular, support
 for the persistent memory instructions for flushing changes to pmem is
 provided. (EXPERIMENTAL)

Package: libpmem
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Persistent Memory low level support library
 libpmem provides low level persistent memory support. In particular, support
 for the persistent memory instructions for flushing changes to pmem is
 provided.

Package: libpmem-dev
Section: libdevel
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmem
 libpmem provides low level persistent memory support. In particular, support
 for the persistent memory instructions for flushing changes to pmem is
 provided.

Package: libpmemblk
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Persistent Memory block array support library
 libpmemblk implements a pmem-resident array of blocks, all the same size, where
 a block is updated atomically with respect to power failure or program
 interruption (no torn blocks).

Package: libpmemblk-dev
Section: libdevel
Architecture: any
Depends: libpmemblk (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemblk
 libpmemblk implements a pmem-resident array of blocks, all the same size, where
 a block is updated atomically with respect to power failure or program
 interruption (no torn blocks).

Package: libpmemlog
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Persistent Memory log file support library
 libpmemlog implements a pmem-resident log file.

Package: libpmemlog-dev
Section: libdevel
Architecture: any
Depends: libpmemlog (=\${binary:Version}), libpmem-dev,  \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemlog
 libpmemlog implements a pmem-resident log file.

Package: libpmemobj
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Persistent Memory object store support library
 libpmemobj turns a persistent memory file into a flexible object store,
 supporting transactions, memory management, locking, lists, and a number of
 other features.

Package: libpmemobj-dev
Section: libdevel
Architecture: any
Depends: libpmemobj (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemobj
 libpmemobj turns a persistent memory file into a flexible object store,
 supporting transactions, memory management, locking, lists, and a number of
 other features.
 .
 This package contains libraries and header files used for linking programs
 against libpmemobj.

Package: libpmempool
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Persistent Memory pool management support library
 libpmempool provides a set of utilities for management, diagnostics and repair
 of persistent memory pools. A pool in this context means a pmemobj pool,
 pmemblk pool, pmemlog pool or BTT layout, independent of the underlying
 storage. The libpmempool is for applications that need high reliability or
 built-in troubleshooting. It may be useful for testing and debugging purposes
 also.

Package: libpmempool-dev
Section: libdevel
Architecture: any
Depends: libpmempool (=\${binary:Version}), libpmem-dev, \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmempool
 libpmempool provides a set of utilities for management, diagnostics and repair
 of persistent memory pools.
 .
 This package contains libraries and header files used for linking programs
 against libpmempool.

Package: $PACKAGE_NAME-dbg
Section: debug
Priority: optional
Architecture: any
Depends: libpmem (=\${binary:Version}), libpmemblk (=\${binary:Version}), libpmemlog (=\${binary:Version}), libpmemobj (=\${binary:Version}), libpmempool (=\${binary:Version}), \${misc:Depends}
Description: Debug symbols for PMDK libraries
 Debug symbols for all PMDK libraries.

Package: pmempool
Section: misc
Architecture: any
Priority: optional
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Standalone utility for management and off-line analysis
 of Persistent Memory pools created by PMDK libraries. It provides a set
 of utilities for administration and diagnostics of Persistent Memory pools.
 Pmempool may be useful for troubleshooting by system administrators
 and users of the applications based on PMDK libraries.

Package: pmreorder
Section: misc
Architecture: any
Priority: optional
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Standalone tool which is a collection of python scripts designed
 to parse and replay operations logged by pmemcheck - a persistent memory
 checking tool. Pmreorder performs the store reordering between persistent
 memory barriers - a sequence of flush-fence operations. It uses a
 consistency checking routine provided in the command line options to check
 whether files are in a consistent state.
EOF

cp LICENSE debian/copyright

if [ -n "$NDCTL_ENABLE" ]; then
	pass_ndctl_enable="NDCTL_ENABLE=$NDCTL_ENABLE"
else
	pass_ndctl_enable=""
fi

cat << EOF > debian/rules
#!/usr/bin/make -f
#export DH_VERBOSE=1
%:
	dh \$@

override_dh_strip:
	dh_strip --dbg-package=$PACKAGE_NAME-dbg

override_dh_auto_build:
	dh_auto_build -- EXPERIMENTAL=${EXPERIMENTAL} prefix=/$PREFIX libdir=/$LIB_DIR includedir=/$INC_DIR docdir=/$DOC_DIR man1dir=/$MAN1_DIR man3dir=/$MAN3_DIR man5dir=/$MAN5_DIR man7dir=/$MAN7_DIR sysconfdir=/etc bashcompdir=/usr/share/bash-completion/completions NORPATH=1 ${pass_ndctl_enable} SRCVERSION=$SRCVERSION

override_dh_auto_install:
	dh_auto_install -- EXPERIMENTAL=${EXPERIMENTAL} prefix=/$PREFIX libdir=/$LIB_DIR includedir=/$INC_DIR docdir=/$DOC_DIR man1dir=/$MAN1_DIR man3dir=/$MAN3_DIR man5dir=/$MAN5_DIR man7dir=/$MAN7_DIR sysconfdir=/etc bashcompdir=/usr/share/bash-completion/completions NORPATH=1 ${pass_ndctl_enable} SRCVERSION=$SRCVERSION
	find -path './debian/*usr/share/man/man*/*.gz' -exec gunzip {} \;

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
$MAN5_DIR/poolset.5
EOF

cat $MAGIC_INSTALL > debian/libpmem.postinst
sed -i '1s/.*/\#\!\/bin\/bash/' debian/libpmem.postinst
echo $'\n#DEBHELPER#\n' >> debian/libpmem.postinst
cat $MAGIC_UNINSTALL > debian/libpmem.prerm
sed -i '1s/.*/\#\!\/bin\/bash/' debian/libpmem.prerm
echo $'\n#DEBHELPER#\n' >> debian/libpmem.prerm

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
$MAN7_DIR/libpmem.7
$MAN3_DIR/pmem_*.3
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
# pmdk provides second set of libraries for debugging.
# These are in /usr/lib/$arch/pmdk_dbg/, but still trigger ldconfig.
# Related issue: https://github.com/pmem/issues/issues/841
libpmem-dev: package-has-unnecessary-activation-of-ldconfig-trigger

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
$MAN7_DIR/libpmemblk.7
$MAN3_DIR/pmemblk_*.3
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
# pmdk provides second set of libraries for debugging.
# These are in /usr/lib/$arch/pmdk_dbg/, but still trigger ldconfig.
# Related issue: https://github.com/pmem/issues/issues/841
libpmemblk-dev: package-has-unnecessary-activation-of-ldconfig-trigger
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
$MAN7_DIR/libpmemlog.7
$MAN3_DIR/pmemlog_*.3
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
# pmdk provides second set of libraries for debugging.
# These are in /usr/lib/$arch/pmdk_dbg/, but still trigger ldconfig.
# Related issue: https://github.com/pmem/issues/issues/841
libpmemlog-dev: package-has-unnecessary-activation-of-ldconfig-trigger
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
$MAN7_DIR/libpmemobj.7
$MAN3_DIR/pmemobj_*.3
$MAN3_DIR/pobj_*.3
$MAN3_DIR/oid_*.3
$MAN3_DIR/toid*.3
$MAN3_DIR/direct_*.3
$MAN3_DIR/d_r*.3
$MAN3_DIR/tx_*.3
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
# pmdk provides second set of libraries for debugging.
# These are in /usr/lib/$arch/pmdk_dbg/, but still trigger ldconfig.
# Related issue: https://github.com/pmem/issues/issues/841
libpmemobj-dev: package-has-unnecessary-activation-of-ldconfig-trigger
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
$MAN7_DIR/libpmempool.7
$MAN3_DIR/pmempool_*.3
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
# pmdk provides second set of libraries for debugging.
# These are in /usr/lib/$arch/pmdk_dbg/, but still trigger ldconfig.
# Related issue: https://github.com/pmem/issues/issues/841
libpmempool-dev: package-has-unnecessary-activation-of-ldconfig-trigger
EOF

cat << EOF > debian/$PACKAGE_NAME-dbg.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF

cat << EOF > debian/pmempool.install
usr/bin/pmempool
$MAN1_DIR/pmempool.1
$MAN1_DIR/pmempool-*.1
usr/share/bash-completion/completions/pmempool
EOF

cat << EOF > debian/pmempool.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF

cat << EOF > debian/pmreorder.install
usr/bin/pmreorder
usr/share/pmreorder/*.py
$MAN1_DIR/pmreorder.1
EOF

cat << EOF > debian/pmreorder.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF

# librpmem & rpmemd
if [ "${BUILD_RPMEM}" = "y" -a "${RPMEM_DPKG}" = "y" ]
then
	append_rpmem_control;
	rpmem_install_triggers_overrides;
fi

# daxio
if [ "${NDCTL_ENABLE}" != "n" ]
then
	append_daxio_control;
	daxio_install_triggers_overrides;
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
	--preserve-envvar=NDCTL_ENABLE \
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
