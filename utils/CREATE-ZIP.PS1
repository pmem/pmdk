#
# Copyright 2016-2019, Intel Corporation
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
# CREATE-ZIP.PS1 -- script to create release zip package
#

#
# parameter handling
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("b")]
    $build = "debug",
    [alias("v")]
    $version = "0",
    [alias("e")]
    $extended = "0"
    )

$scriptdir = Split-Path -Parent $PSCommandPath
$rootdir = $scriptdir + "\..\"

$builddir = $rootdir + "\src\x64\"
$zipdir = $builddir + "\pmdk\"

if ($version -eq "0") {
    $git = Get-Command -Name git -ErrorAction SilentlyContinue
    if ($git) {
        $version = $(git describe)
    } else {
        $version = "0"
    }
}

$zipfile = $builddir + "\pmdk-" + $version + "-win-x64-" + $build + ".zip"

Remove-Item $zipdir -Force -Recurse -ea si
Get-ChildItem | Where-Object {$_.Name -Match "pmdk-.*-win-x64.zip"} | Remove-Item -Force -ea si
New-Item -ItemType directory -Path ( $zipdir) -Force | Out-Null
New-Item -ItemType directory -Path ( $zipdir + "\bin\") -Force | Out-Null
New-Item -ItemType directory -Path ( $zipdir + "\lib\") -Force | Out-Null

$libs = @("libpmem", "libpmemblk", "libpmemlog", "libpmemobj", "libpmempool",
    "libvmem")
$apps = @("pmempool")
$apps_extended =  @("pmempool", "pmemalloc", "pmemdetect", "pmemspoil", "pmemwrite")

if ($extended -eq "1") {
    $apps = $apps_extended
}

foreach ($lib in $libs) {
    Copy-Item ($builddir + $build + "\libs\" + $lib + ".dll") ($zipdir + "\bin\")
    foreach ($ex in @(".lib", ".pdb")) {
        Copy-Item ($builddir + $build + "\libs\" + $lib + $ex) ($zipdir + "\lib\")
    }
}
foreach ($app in $apps) {
	if ($app -eq "pmempool") {
		Copy-Item ($builddir + $build + "\libs\" + $app + ".exe") ($zipdir + "\bin\")
		Copy-Item ($builddir + $build + "\libs\" + $app + ".pdb") ($zipdir + "\lib\")
	} else {
		Copy-Item ($builddir + $build + "\tests\" + $app + ".exe") ($zipdir + "\bin\")
		Copy-Item ($builddir + $build + "\tests\" + $app + ".pdb") ($zipdir + "\lib\")
	}
}

Copy-Item -Recurse ($rootdir + "src\include") ($zipdir)
Remove-Item -Force ($zipdir + "include\.cstyleignore")
Remove-Item -Force ($zipdir + "include\README")
Remove-Item -Force ($zipdir + "include\librpmem.h")
Remove-Item -Force ($zipdir + "include\libvmmalloc.h")
Copy-Item ($rootdir + "README.md") ($zipdir)
Copy-Item ($rootdir + "LICENSE") ($zipdir)
Copy-Item ($rootdir + "ChangeLog") ($zipdir)

Add-Type -Assembly System.IO.Compression.FileSystem
$comprlevel = [System.IO.Compression.CompressionLevel]::Optimal

if (Test-Path  ($zipdir)) {
    [System.IO.Compression.ZipFile]::CreateFromDirectory($zipdir, $zipfile,
        $comprlevel, $true)
}

Remove-Item $zipdir -Force -Recurse -ea si
