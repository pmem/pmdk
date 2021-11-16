//
// Copyright 2020, Intel Corporation
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//
//     * Neither the name of the copyright holder nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

// This file contains code for loading Jenkins libraries

echo "Loading pipeline libraries"

// Define globally scoped `libs` object - it will be accessible for usage after executing code from this file:
libs = [:]

// Set directory containing this file as a library path for the rest of libs:
def lib_path = new File(loading_libraries_code).parent

// First - load OS related libraries, as the rest code will use them:
// exclusive Linux and Windows libs:
libs.linux = load "${lib_path}/os/linux.groovy"
libs.windows = load "${lib_path}/os/windows.groovy"

// and common Linux/Windows libs:
libs.common_linux = load "${lib_path}/os/common/linux.groovy"
libs.common_windows = load "${lib_path}/os/common/windows.groovy"

// Assign to libs.os library related to the OS, based on the node's OS.
// `libs.os` should be your default choice when calling OS-related code;
// in the average pipeline we'll always have proper library - windows.groovy or linux.groovy - bound to libs.os.
// If autodetection based on node's OS won't suit your needs, there will always be libs.common_linux or libs.common_windows
// available for explicit calls when necessary.
libs.os = isUnix() ? libs.common_linux : libs.common_windows

// then load utils libs:
libs.utils = load "${lib_path}/utils/utils.groovy"
libs.awx = load "${lib_path}/utils/awx.groovy"
libs.snapshots = load "${lib_path}/utils/snaphots.groovy"

// then pipeline related libs:
libs.unittests = load "${lib_path}/pipeline/unittests.groovy"
libs.ras = load "${lib_path}/pipeline/ras.groovy"
