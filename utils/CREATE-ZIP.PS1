# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation
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

$libs = @("libpmem", "libpmemblk", "libpmemlog", "libpmemobj", "libpmempool")
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
