#!/bin/bash
#
# Copyright 2016, Intel Corporation
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
# make-pkg-config.sh - script for building pkg-config files
#

if [ $# != "3" ]
then
	echo "usage: $0 <prefix> <libdir> <version>"
	exit 1
fi

prefix=$1
libdir=$2
version=$3

cat << EOF > libpmem.pc
prefix=${prefix}
libdir=${libdir}
version=${version}
includedir=\${prefix}/include

Name: libpmem
Description: libpmem library from NVML project
Version: \${version}
URL: http://pmem.io/nvml
Requires:
Libs: -L\${libdir} -lpmem
Cflags: -I\${includedir}
EOF

cat << EOF > libpmemobj.pc
prefix=${prefix}
libdir=${libdir}
version=${version}
includedir=\${prefix}/include

Name: libpmemobj
Description: libpmemobj library from NVML project
Version: \${version}
URL: http://pmem.io/nvml
Requires: libpmem
Libs: -L\${libdir} -lpmemobj
Cflags: -I\${includedir}
EOF

cat << EOF > libpmempool.pc
prefix=${prefix}
libdir=${libdir}
version=${version}
includedir=\${prefix}/include

Name: libpmempool
Description: libpmempool library from NVML project
Version: \${version}
URL: http://pmem.io/nvml
Requires: libpmem
Libs: -L\${libdir} -lpmempool
Cflags: -I\${includedir}
EOF

cat << EOF > libpmemblk.pc
prefix=${prefix}
libdir=${libdir}
version=${version}
includedir=\${prefix}/include

Name: libpmemblk
Description: libpmemblk library from NVML project
Version: \${version}
URL: http://pmem.io/nvml
Requires: libpmem
Libs: -L\${libdir} -lpmemblk
Cflags: -I\${includedir}
EOF

cat << EOF > libpmemlog.pc
prefix=${prefix}
libdir=${libdir}
version=${version}
includedir=\${prefix}/include

Name: libpmemlog
Description: libpmemlog library from NVML project
Version: \${version}
URL: http://pmem.io/nvml
Requires: libpmem
Libs: -L\${libdir} -lpmemlog
Cflags: -I\${includedir}
EOF

cat << EOF > libvmem.pc
prefix=${prefix}
libdir=${libdir}
version=${version}
includedir=\${prefix}/include

Name: libvmem
Description: libvmem library from NVML project
Version: \${version}
URL: http://pmem.io/nvml
Requires:
Libs: -L\${libdir} -lvmem
Cflags: -I\${includedir}
EOF

cat << EOF > libvmmalloc.pc
prefix=${prefix}
libdir=${libdir}
version=${version}
includedir=\${prefix}/include

Name: libvmmalloc
Description: libvmmalloc library from NVML project
Version: \${version}
URL: http://pmem.io/nvml
Requires:
Libs: -L\${libdir} -lvmmalloc
Cflags: -I\${includedir}
EOF

cat << EOF > libpmemobj++.pc
prefix=${prefix}
libdir=${libdir}
version=${version}
includedir=\${prefix}/include/libpmemobj

Name: libpmemobj++
Description: C++ bindings for the libpmemobj library from NVML project
Version: \${version}
URL: http://pmem.io/nvml
Requires.private:
Libs: -L\${libdir} -lpmemobj
Cflags: -I\${includedir}
EOF
