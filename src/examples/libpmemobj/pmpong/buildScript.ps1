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
# buildScript.ps1 -- script for downloading and copying SFML files
#
$url = "https://www.sfml-dev.org/files/SFML-2.4.2-windows-vc14-64-bit.zip"
$Filename = [System.IO.Path]::GetFileName($url)
$path = "$env:TEMP\$Filename"
$webClient = new-object System.Net.WebClient
$webClient.DownloadFile($url,$path)
$SFMLFileExists = Test-Path $env:TEMP\SFML-2.4.2
if ($SFMLFileExists -eq $False) {
  $shell = New-Object -ComObject shell.application
  $zip = $shell.NameSpace($path)
  foreach ($item in $zip.items()) {
    $shell.Namespace($env:TEMP).CopyHere($item)
  }
}
Copy-Item $env:TEMP\SFML-2.4.2\bin\* -Destination $env:TargetDir
Copy-Item $env:TEMP\SFML-2.4.2\include -Destination $env:TargetDir -recurse
Copy-Item $env:TEMP\SFML-2.4.2\lib -Destination $env:TargetDir -recurse
