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
# CREATE-ZIP.PS1 -- script to create release zip package
#

$build_dir = '..\src\x64\'

$target_dir = 'nvml-windows\'
Remove-Item $target_dir -Force -Recurse -ea si
Get-ChildItem | Where{$_.Name -Match "nvml-.*-win-x64.zip"} | remove-item -Force -ea si
New-Item -ItemType directory -Path ( $target_dir) -Force | Out-Null
New-Item -ItemType directory -Path ( $target_dir + "\bin\") -Force | Out-Null
New-Item -ItemType directory -Path ( $target_dir + "\lib\") -Force | Out-Null
New-Item -ItemType directory -Path ( $target_dir + "\bin\debug\") -Force | Out-Null
New-Item -ItemType directory -Path ( $target_dir + "\lib\debug\") -Force | Out-Null


$libs = @("libpmem", "libpmemblk", "libpmemlog", "libpmemobj", "libpmempool")
$apps = @("pmempool")

$build = "debug"
foreach ($lib in $libs) {
    Copy-Item ($build_dir + $build + "\" + $lib + ".dll") ($target_dir + "\bin\" +$build + "\")
    foreach($ex in @(".lib", ".pdb")) {
        Copy-Item ($build_dir + $build + "\" + $lib + $ex) ($target_dir + "\lib\" + $build + "\")
    }
}
foreach ($app in $apps) {
    Copy-Item ($build_dir + $build + "\" + $app + ".exe") ($target_dir + "\bin\" +$build + "\")
    Copy-Item ($build_dir + $build + "\" + $app + ".pdb") ($target_dir + "\lib\" + $build + "\")
}
$build = "release"
foreach ($lib in $libs) {
    Copy-Item ($build_dir + $build + "\" + $lib + ".dll") ($target_dir + "\bin\" + "\")
    foreach($ex in @(".lib", ".pdb")) {
        Copy-Item ($build_dir + $build + "\" + $lib + $ex) ($target_dir + "\lib\" + "\")
    }
}
foreach ($app in $apps) {
    Copy-Item ($build_dir + $build + "\" + $app + ".exe") ($target_dir + "\bin\" + "\")
    Copy-Item ($build_dir + $build + "\" + $app + ".pdb") ($target_dir + "\lib\" + "\")
}

Copy-Item -Recurse ("..\src\include") ($target_dir)
Remove-Item -Force ($target_dir + "include\README")
Remove-Item -Force ($target_dir + "include\librpmem.h")
Remove-Item -Force ($target_dir + "include\libvmmalloc.h")
Copy-Item ("..\README.md") ($target_dir)
Copy-Item ("..\LICENSE") ($target_dir)
Copy-Item ("..\ChangeLog") ($target_dir)

Add-Type -assembly "system.io.compression.filesystem"

$git = Get-Command -Name git -ErrorAction SilentlyContinue

if ($git) {
    $ver = $(git describe)
} else {
    $ver = "0"
}
[io.compression.zipfile]::CreateFromDirectory("nvml-windows", "nvml-" + $ver + "-win-x64.zip")
Remove-Item $target_dir -Force -Recurse -ea si
