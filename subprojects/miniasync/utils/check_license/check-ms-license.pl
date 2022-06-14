#!/usr/bin/perl -w
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

use Digest::MD5 "md5_hex";

my $BSD3 = <<EndText;
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
EndText

# a runaway sed job may alter this file too
md5_hex($BSD3) eq "066df8826723b6db407a931e5d6084f4"
	or die "Text of BSD3 license has been corrupted.\n";

my $err = 0;

$BSD3 =~ s/^/ /mg;		# indent the text
$BSD3 =~ s/\n \n/\n\n/sg;	# except for empty lines

undef $/;
for my $f (@ARGV) {
	next unless -f $f;
	open F, '<', $f or die "Can't read ｢$f｣\n";
	$_ = <F>;
	close F;
	next unless /Copyright.*(Microsoft Corporation|FUJITSU)/;

	s/^ \*//mg;
	s/^#//mg;
	if (index($_, $BSD3) == -1) {
		$err = 1;
		print STDERR "Outside copyright but no/wrong license text in $f\n";
	}
}

exit $err
