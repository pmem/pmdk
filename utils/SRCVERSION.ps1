#
# Copyright 2016-2020, Intel Corporation
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
# SRCVERSION.PS1 -- script to create SCRVERSION macro and generate srcversion.h
#

#
# Windows dll versioning supports only fixed number of fields. The most
# important are MAJOR, MINOR and REVISION. We have 3-compoment releases
# (e.g. 1.5.1) with release candidates, so we have to encode this information
# into this fixed number of fields. That's why we abuse REVISION to encode both
# 3rd component and rc status.
# REVISION = 3RDCOMP * 1000 + (!is_rc) * 100 + rc.
#
# Examples:
# +---------------------+-----+-----+--------+-----+------+-------+----------+
# |git describe --long  |MAJOR|MINOR|REVISION|BUILD|BUGFIX|PRIVATE|PRERELEASE|
# +---------------------+-----+-----+--------+-----+------+-------+----------+
# |1.5-rc2-0-12345678   |    1|    5|       2|    0| false|  false|      true|
# |1.5-rc3-6-12345678   |    1|    5|       3|    6| false|   true|      true|
# |1.5-0-12345678       |    1|    5|     100|    0| false|  false|     false|
# |1.5-6-123345678      |    1|    5|     100|    6| false|   true|     false|
# |1.5.2-rc1-0-12345678 |    1|    5|    2001|    0|  true|  false|      true|
# |1.5.2-rc4-6-12345678 |    1|    5|    2004|    6|  true|   true|      true|
# |1.5.2-0-12345678     |    1|    5|    2100|    0|  true|  false|     false|
# |1.5.2-6-12345678     |    1|    5|    2100|    6|  true|   true|     false|
# +---------------------+-----+-----+--------+-----+------+-------+----------+
#

$scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
$file_path = $scriptPath + "\..\src\windows\include\srcversion.h"
$git_version_file = $scriptPath + "\..\GIT_VERSION"
$version_file = $scriptPath + "\..\VERSION"
$git = Get-Command -Name git -ErrorAction SilentlyContinue

if (Test-Path $file_path) {
    $old_src_version = Get-Content $file_path | `
        Where-Object { $_ -like '#define SRCVERSION*' }
} else {
    $old_src_version = ""
}

$git_version = ""
$git_version_hash = ""

if (Test-Path $git_version_file) {
    $git_version = Get-Content $git_version_file
    if ($git_version -eq "`$Format:%h`$") {
        $git_version = ""
    } else {
        $git_version_hash = $git_version
    }
}

$PRERELEASE = $false
$BUGFIX = $false
$PRIVATE = $true
$CUSTOM = $false

if ($null -ne $args[0]) {
    $version = $args[0]
    $ver_array = $version.split("-+")
} elseif (Test-Path $version_file) {
    $version = Get-Content $version_file
    $ver_array = $version.split("-+")
} elseif ($git_version_hash -ne "") {
    $MAJOR = 0
    $MINOR = 0
    $REVISION = 0
    $BUILD = 0

    $version = $git_version_hash
    $CUSTOM = $true
    $version_custom_msg = "#define VERSION_CUSTOM_MSG `"$git_version_hash`""
} elseif ($null -ne $git) {
    $version = $(git describe)
    $ver_array = $(git describe --long).split("-+")
} else {
    $MAJOR = 0
    $MINOR = 0
    $REVISION = 0
    $BUILD = 0

    $version = "UNKNOWN_VERSION"
    $CUSTOM = $true
    $version_custom_msg = "#define VERSION_CUSTOM_MSG `"UNKNOWN_VERSION`""
}

if ($null -ne $ver_array) {
    $ver_dots = $ver_array[0].split(".")
    $MAJOR = $ver_dots[0]
    $MINOR = $ver_dots[1]
    if ($ver_dots.length -ge 3) {
        $REV = $ver_dots[2]
        $BUGFIX = $true
    } else {
        $REV = 0
    }

    $REVISION = 1000 * $REV
    $BUILD = $ver_array[$ver_array.length - 2]

    if ($ver_array.length -eq 4) {
        # <MAJOR>.<MINOR>[.<BUGFIX>]-<SUFFIX><REVISION>-<BUILD>-<HASH>

        if ($ver_array[1].StartsWith("rc")) {
            # <MAJOR>.<MINOR>[.<BUGFIX>]-rc<REVISION>-<BUILD>-<HASH>
            $REVISION += $ver_array[1].Substring("rc".Length)
            $PRERELEASE = $true
            $version = "$($ver_array[0])-$($ver_array[1])+git$($ver_array[2]).$($ver_array[3])"
        } else {
            # <MAJOR>.<MINOR>[.<BUGFIX>]-<SOMETHING>-<BUILD>-<HASH>
            throw "Unknown version format"
        }
    } else {
        # <MAJOR>.<MINOR>[.<BUGFIX>]-<BUILD>-<HASH>
        $REVISION += 100
        $version = "$($ver_array[0])+git$($ver_array[1]).$($ver_array[2])"
    }

    if ($BUILD -eq 0) {
        # it is not a (pre)release build
        $PRIVATE = $false
    }
}

$src_version = "#define SRCVERSION `"$version`""

if ($old_src_version -eq $src_version) {
    exit 0
}

Write-Output "updating source version: $version"
Write-Output $src_version > $file_path

Write-Output "#ifdef RC_INVOKED" >> $file_path

Write-Output "#define MAJOR $MAJOR" >> $file_path
Write-Output "#define MINOR $MINOR" >> $file_path
Write-Output "#define REVISION $REVISION" >> $file_path
Write-Output "#define BUILD $BUILD" >> $file_path

if ($PRERELEASE) {
    Write-Output "#define PRERELEASE 1"  >> $file_path
}
if ($BUGFIX) {
    Write-Output "#define BUGFIX 1"  >> $file_path
}
if ($PRIVATE) {
    Write-Output "#define PRIVATE 1"  >> $file_path
}
if ($CUSTOM) {
    Write-Output "#define CUSTOM 1"  >> $file_path
    Write-Output $version_custom_msg  >> $file_path
}

Write-Output "#endif" >> $file_path
