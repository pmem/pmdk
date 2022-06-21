# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# TEST_SPARSE.ps1 -- compare performance of various methods of creating
#                    sparse files on Windows
#
# usage: .\TEST_SPARSE.ps1 filename length repeats
#

if ($args.count -lt 3) {
	Write-Error "usage: sparse.ps1 filename length repeats"
	exit 1
}

$path = $args[0]
$size = $args[1]
$count = $args[2]

#
# epoch -- get timestamp
#
function epoch {
	return [int64](([datetime]::UtcNow)-(get-date "1/1/1970")).TotalMilliseconds
}

#
# remove_file -- remove file if exists
#
function remove_file {
	if (test-path $args[0]) {
		rm -force $args[0]
	}
}

#
# create_holey_file1 -- create sparse file using 'sparsefile' utility
#
function create_holey_file1 {
	$fname = $args[0]
	$size = $args[1]

	& '..\..\..\x64\debug\sparsefile.exe' $fname $size
	if ($LASTEXITCODE -ne 0) {
		Write-Error "Error $LASTEXITCODE with sparsefile create"
		exit $LASTEXITCODE
	}
	Write-Host -NoNewline "."
}

#
# create_holey_file2 -- create sparse file using 'powershell' & 'fsutil'
#
function create_holey_file2 {
	$fname = $args[0]
	$size = $args[1]

	$f = [System.IO.File]::Create($fname)
	$f.Close()

	# XXX: How to mark file as sparse using pure PowerShell API?
	# Setting 'SparseFile' attribute in PS does not work for some reason.

	# mark file as sparse
	& fsutil sparse setflag $path

	$f = [System.IO.File]::Open($fname, "Append")
	$f.SetLength($size)
	$f.Close()

	Write-Host -NoNewline "."
}

#
# create_holey_file3 -- create sparse file using 'fsutil'
#
function create_holey_file3 {
	$fname = $args[0]
	$size = $args[1]

	& "FSUtil" File CreateNew $fname $size
	if ($LASTEXITCODE -ne 0) {
		Write-Error "Error $LASTEXITCODE with FSUTIL create"
		exit $LASTEXITCODE
	}
	& "FSUtil" Sparse SetFlag $fname
	if ($LASTEXITCODE -ne 0) {
		Write-Error "Error $LASTEXITCODE with FSUTIL setFlag"
		exit $LASTEXITCODE
	}
	& "FSUtil" Sparse SetRange $fname 0 $size
	if ($LASTEXITCODE -ne 0) {
		Write-Error "Error $LASTEXITCODE with FSUTIL setRange"
		exit $LASTEXITCODE
	}
	Write-Host -NoNewline "."
}

$start = epoch
for ($i=1;$i -lt $count;$i++) {
	remove_file $path
	create_holey_file1 $path $size
}
$end = epoch
$t = ($end - $start) / 1000
Write-Host "`nsparsefile: $t seconds"

$start = epoch
for ($i=1;$i -lt $count;$i++) {
	remove_file $path
	create_holey_file2 $path $size
}
$end = epoch
$t = ($end - $start) / 1000
Write-Host "`npowershell + fsutil: $t seconds"

$start = epoch
for ($i=1;$i -lt $count;$i++) {
	remove_file $path
	create_holey_file3 $path $size
}
$end = epoch
$t = ($end - $start) / 1000
Write-Host "`nfsutil: $t seconds"
