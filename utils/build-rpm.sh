#!/bin/bash -e
#
# Copyright (c) 2014, Intel Corporation
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
#     * Neither the name of Intel Corporation nor the names of its
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
# build-rmp.sh - Script for building rpm packages
#

SCRIPT_DIR=$(dirname $0)
source $SCRIPT_DIR/pkg-common.sh

if [ "$#" != "4" ]
then
	echo "Usage: $(basename $0) VERSION_TAG SOURCE_DIR WORKING_DIR OUT_DIR"
	exit 1
fi

PACKAGE_VERSION_TAG=$1
SOURCE=$2
WORKING_DIR=$3
OUT_DIR=$4

function convert_changelog() {
	while read
	do
		if [[ $REPLY =~ $REGEX_DATE_AUTHOR ]]
		then
			DATE=$(date --date="${BASH_REMATCH[1]}" '+%a %b %d %Y')
			AUTHOR="${BASH_REMATCH[2]}"
			echo "* ${DATE} ${AUTHOR}"
		elif [[ $REPLY =~ $REGEX_MESSAGE_START ]]
		then
			echo "- ${BASH_REMATCH[1]}"
		elif [[ $REPLY =~ $REGEX_MESSAGE ]]
		then
			echo "  ${BASH_REMATCH[1]}"
		fi
	done < $1
}

check_tool rpmbuild
check_file $SCRIPT_DIR/pkg-config.sh

source $SCRIPT_DIR/pkg-config.sh

PACKAGE_VERSION=$(get_version $PACKAGE_VERSION_TAG)
# Release tag can be thought of as the package's version - not source.
# Therefore we set this value to 1 as we always build package from well
# defined version of the source. This should be incremented when rebuilding
# package with any patches specified for target platform/distro.
PACKAGE_RELEASE=1

if [ -z "$PACKAGE_VERSION" ]
then
	error "Can not parse version from '${PACKAGE_VERSION_TAG}'"
	exit 1
fi

PACKAGE_SOURCE=${PACKAGE_NAME}-${PACKAGE_VERSION}
SOURCE=$PACKAGE_NAME
PACKAGE_TARBALL=$PACKAGE_SOURCE.tar.gz
RPM_SPEC_FILE=$PACKAGE_SOURCE/$PACKAGE_NAME.spec
CHANGELOG_FILE=$PACKAGE_SOURCE/ChangeLog
OLDPWD=$PWD

[ -d $WORKING_DIR ] || mkdir -v $WORKING_DIR
[ -d $OUT_DIR ] || mkdir $OUT_DIR

cd $WORKING_DIR

check_dir $SOURCE

mv $SOURCE $PACKAGE_SOURCE

#
# Create parametrized spec file required by rpmbuild.
# Most of variables are set in config.sh file in order to
# keep descriptive values separately from this script.
#
cat << EOF > $RPM_SPEC_FILE
%if 0%{?suse_version} != 0
%define package_group System
%define dist .suse%{suse_version}
%else
%define package_group System Environment
%endif

Name:		$PACKAGE_NAME
Version:	$PACKAGE_VERSION
Release:	$PACKAGE_RELEASE%{?dist}
Summary:	$PACKAGE_SUMMARY
Packager:	$PACKAGE_MAINTAINER
Group:		%{package_group}/Libraries
License:	Intel BSD
URL:		$PACKAGE_URL

Source0:	$PACKAGE_TARBALL
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	gcc glibc-devel
BuildRequires:	autoconf, automake, make
BuildRequires:	libuuid-devel
BuildArch:	x86_64

%description
$PACKAGE_DESCRIPTION

%package -n libpmem
Summary: libpmem library
Group: %{package_group}/Libraries
%description -n libpmem
NVML libpmem library

%files -n libpmem
%defattr(-,root,root,-)
%{_libdir}/libpmem.so.*

%package -n libpmem-devel
Summary: libpmem development library
Group: Development/Libraries
%description -n libpmem-devel
Development files for NVML libpmem library

%files -n libpmem-devel
%defattr(-,root,root,-)
%{_libdir}/libpmem.so
%{_libdir}/libpmem.a
%{_libdir}/nvml_debug/libpmem.so
%{_libdir}/nvml_debug/libpmem.so.*
%{_libdir}/nvml_debug/libpmem.a
/usr/include/libpmem.h
/usr/share/man/man3/libpmem.3.gz

%package -n libpmemblk
Summary: libpmemblk library
Group: %{package_group}/Libraries
%description -n libpmemblk
NVML libpmemblk library

%files -n libpmemblk
%defattr(-,root,root,-)
%{_libdir}/libpmemblk.so.*

%package -n libpmemblk-devel
Summary: libpmemblk development library
Group: Development/Libraries
%description -n libpmemblk-devel
Development files for NVML libpmemblk library

%files -n libpmemblk-devel
%defattr(-,root,root,-)
%{_libdir}/libpmemblk.so
%{_libdir}/libpmemblk.a
%{_libdir}/nvml_debug/libpmemblk.so
%{_libdir}/nvml_debug/libpmemblk.so.*
%{_libdir}/nvml_debug/libpmemblk.a
/usr/include/libpmemblk.h
#/usr/share/man/man3/libpmemblk.3.gz

%package -n libpmemlog
Summary: libpmemlog library
Group: %{package_group}/Libraries
%description -n libpmemlog
NVML libpmemlog library

%files -n libpmemlog
%defattr(-,root,root,-)
%{_libdir}/libpmemlog.so.*

%package -n libpmemlog-devel
Summary: libpmemlog development library
Group: Development/Libraries
%description -n libpmemlog-devel
Development files for NVML libpmemlog library

%files -n libpmemlog-devel
%defattr(-,root,root,-)
%{_libdir}/libpmemlog.so
%{_libdir}/libpmemlog.a
%{_libdir}/nvml_debug/libpmemlog.so
%{_libdir}/nvml_debug/libpmemlog.so.*
%{_libdir}/nvml_debug/libpmemlog.a
/usr/include/libpmemlog.h
#/usr/share/man/man3/libpmemlog.3.gz

%package -n libpmemobj
Summary: libpmemobj library
Group: %{package_group}/Libraries
%description -n libpmemobj
NVML libpmemobj library

%files -n libpmemobj
%defattr(-,root,root,-)
%{_libdir}/libpmemobj.so.*

%package -n libpmemobj-devel
Summary: libpmemobj development library
Group: Development/Libraries
%description -n libpmemobj-devel
Development files for NVML libpmemobj library

%files -n libpmemobj-devel
%defattr(-,root,root,-)
%{_libdir}/libpmemobj.so
%{_libdir}/libpmemobj.a
%{_libdir}/nvml_debug/libpmemobj.so
%{_libdir}/nvml_debug/libpmemobj.so.*
%{_libdir}/nvml_debug/libpmemobj.a
/usr/include/libpmemobj.h
#/usr/share/man/man3/libpmemobj.3.gz

%package -n libvmem
Summary: libvmem library
Group: %{package_group}/Libraries
%description -n libvmem
NVML libvmem library

%files -n libvmem
%defattr(-,root,root,-)
%{_libdir}/libvmem.so.*

%package -n libvmem-devel
Summary: libvmem development library
Group: Development/Libraries
%description -n libvmem-devel
Development files for NVML libvmem library

%files -n libvmem-devel
%defattr(-,root,root,-)
%{_libdir}/libvmem.so
%{_libdir}/libvmem.a
%{_libdir}/nvml_debug/libvmem.so
%{_libdir}/nvml_debug/libvmem.so.*
%{_libdir}/nvml_debug/libvmem.a
/usr/include/libvmem.h
/usr/share/man/man3/libvmem.3.gz

%prep
%setup -q -n $PACKAGE_SOURCE

%build
%{__make}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%check
cp src/test/testconfig.sh.example src/test/testconfig.sh
make check

%clean
make clobber

%changelog
EOF

[ -f $CHANGELOG_FILE ] && convert_changelog $CHANGELOG_FILE >> $RPM_SPEC_FILE

tar zcf $PACKAGE_TARBALL $PACKAGE_SOURCE

# Create directory structure for rpmbuild
mkdir -v BUILD SPECS

rpmbuild --define "_topdir `pwd`"\
         --define "_rpmdir ${OUT_DIR}"\
	 --define "_srcrpmdir ${OUT_DIR}"\
         -ta $PACKAGE_TARBALL

echo "Building rpm packages done"

exit 0
