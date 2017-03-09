#
# Copyright 2017, Intel Corporation
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
# set_lib.ps1 -- script to replace Debug/Release paths to *.lib
#

param(
[string]$projectdir,
[string]$configuration
)
Write-Output "Adapting lib paths to $configuration source ..."
$search=""
if ($configuration -eq "Debug") {
	$search="Release"
}
elseif ($configuration -eq "Release") {
	$search="Debug"
}

$vdprojdir="$projectdir"+"\setup.vdproj"
Set-Variable -Name line 0
$regex='^\s*"SourcePath".*\\{0}*?\\.*\.lib"$' -f $search

$content = Get-Content $vdprojdir
$adapted = @()
foreach($row in $content) {
    if($row -match $regex){
		$adapted += $content[$line] -Replace "\\\\$search\\\\", "\\$configuration\\"
    } else {
		$adapted += $content[$line]
	}
$line++;
}
$adapted | Out-File $vdprojdir
