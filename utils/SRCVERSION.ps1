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
# SRCVERSION.PS1 -- script to create SCRVERSION macro on windows
#

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
$file_path = $scriptPath + "\..\src\windows\include\srcversion.h"
$git = Get-Command -Name git -ErrorAction SilentlyContinue

if (Test-Path $file_path) {
    $old_src_version = Get-Content $file_path | Where-Object { $_ -like '#define SRCVERSION*' }
} else {
    $old_src_version = ""
}

$PRERELEASE = $false
$BUGFIX = $false
$PRIVATE = $true
$CUSTOM = $false
if ($git -eq $null) {
    $MAJOR = 0
    $MINOR = 0
    $REVISION = 0
    $BUILD = 0
    $CUSTOM = $true
    $version_custom_msg = "#define VERSION_CUSTOM_MSG `"Git was not installed during a build, couldn't determine library version`" "
} else {
    $version = $(git describe)
    $no_git = $false
    $ver_array = $(git describe --long).split("-")

    if($ver_array.length -eq 4) {
        # <MAJOR>.<MINOR>-RC<REVISION>-<BUILDNUMBER>-<HASH>
        $MAJOR = $ver_array[0].split(".")[0]
        $MINOR = $ver_array[0].split(".")[1]
        $REVISION = $ver_array[1].Substring("rc".Length)
        $BUILD = $ver_array[2]
        $PRERELEASE = $true
    } elseif($ver_array.length -eq 3) {
        if($ver_array[0].split("+").Length -gt 1) {
            # <MAJOR>.<MINOR>+b<REVISION>-<BUILDNUMBER>-<HASH>
            $MAJOR = $ver_array[0].split("+")[0].split(".")[0]
            $MINOR = $ver_array[0].split("+")[0].split(".")[1]
            $REVISION = 1000 + $ver_array[0].split("+")[1].Substring("b".Length)
            $BUILD = $ver_array[1]
            $BUGFIX = $true
        } else {
            # <MAJOR>.<MINOR>-<BUILDNUMBER>-<HASH>
            $MAJOR = $ver_array[0].split(".")[0]
            $MINOR = $ver_array[0].split(".")[1]
            $REVISION = 1000
            $BUILD = $ver_array[1]
        }
    }

    if($BUILD -eq 0 ) {
        # it is not a (pre)release build
       $PRIVATE = $false
    }
}

$src_version = "#define SRCVERSION `"$version`""

if ($old_src_version -eq $src_version) {
    exit 0
}

echo "updating source version: $version"
echo $src_version > $file_path

echo "#ifdef RC_INVOKED" >> $file_path

echo "#define MAJOR $MAJOR" >> $file_path
echo "#define MINOR $MINOR" >> $file_path
echo "#define REVISION $REVISION" >> $file_path
echo "#define BUILD $BUILD" >> $file_path

if($PRERELEASE) {
    echo "#define PRERELEASE 1"  >> $file_path
}

if($BUGFIX) {
    echo "#define BUGFIX 1"  >> $file_path
}

if($PRIVATE) {
    echo "#define PRIVATE 1"  >> $file_path
}

if($CUSTOM) {
    echo "#define CUSTOM 1"  >> $file_path
    echo $version_custom_msg  >> $file_path
}

echo "#endif" >> $file_path



