#
# Copyright 2015-2016, Intel Corporation
# Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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

#
# src/test/obj_pool/TEST6 -- unit test for pmemobj_create
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

$Env:UNITTEST_NAME = "obj_pool\TEST6"
$Env:UNITTEST_NUM = "6"
# XXX:  bash has a few calls to tools that we don't have on
# windows (yet) that set PMEM_IS_PMEM and NON_PMEM_IS_PMEM based
# on their output
$Env:PMEM_IS_PMEM = $true
$Env:NON_PMEM_IS_PMEM = $true

# standard unit test setup
. ..\unittest\unittest.ps1

setup

echo $null >> $DIR\testfile

#
# TEST6 existing file, file length < min required size, poolsize == 0,
#       layout string is too long

# /dev/urandom is not implemented yet in windows
# $ENV:LAYOUT = "dd if=/dev/urandom bs=1 count=768 2>/dev/null | base64 -w 0"
$ENV:LAYOUT = "zXKCykLuPM+ez4Nvm5bkZIhEMp8YoZwCXULiunTLPAV/F1ppMIDeXfUUWl0gleMbb5dWRiolL0MucGn3WD5KWrvhqb3rWxicFtbsjaYkq3sfJe8xAGmnUNRUG0r78pyI0RWCe7GMwdk11szN23W5g8Usd4J7fEdWXVy7EcXVY07GZAT4Mk+ks/11Me7Qi8n/aMwAQDaAgLwvGdDZl68BEriL9qrNZixruasltaxMkWT0ZS24UgT+V9jb7lnKMYhHWtrx+TBu08GWUbE/FvpV3NZbCKOzCcyn2fCsl84e25hJQwDClqPd8d2R1OJO/6r37pW4LX7HKDXh8a/CkFtW4exZio3FYLGw9AAIsVTV+BXZMuj/axNJjoXLJVhC8qrLSHVSBhRR5kfGJLenrKAcX6N0J/Ua+Qag5xBGs3bYNHcT3J7Olk4qIWgP0T8IxNAz8KSgOdIeybrnhi6+uyQUKraaVNJAXUo5m9pOurQLaTqvt0/dclYQ+4LcEFE8Lv25ANzDxwe3QGAQQJwp5JTBfhlsb8V6a2iAZ2ic11tUDECFhOBG+RNXpB8e2O5TDlevfIwfp4pCwNyEiJWtFcO/48w06SnUknIe37eV3E3wQY+CeGFQrUghcAtsqWL7uGEkBDa/m0ESfUEcItUavNcFOU+YTForj9QyJ3jZfc2tSV4CkVLBbxCnKDc2CpG75AEKyLasrhruV6KK6sYq4DIbLMM05BfNSomdGddumiErV0pjCor7deIUwMRwSD3FGtQjp7fMTxWrkDG7DxES4Kqr81p/uRpz4X8cSjYDCJo321N9UUfjOmE76PM4EPYbDsqNbDBomPtvy2zAq/3ivA/s8atxdALYwRgeVpKnVG7vAhnFqiTlie+aCxUO3UrcwLvvwZ635UheqVmnhdgSA8e6xQp0EzdiGAK73BXupd0GqCJx1lDmyP2srXGEH1j/l3YejhhbUCtjifrKOkGr60Yj+3+x5rZNJTyAaCUBWHl5Rw39RvcTjSATtenV7SDyQa3G"

expect_normal_exit $Env:EXE_DIR\obj_pool$Env:EXESUFFIX `
    c $DIR\testfile $ENV:LAYOUT 0 0640

check_files $DIR\testfile

check

pass
