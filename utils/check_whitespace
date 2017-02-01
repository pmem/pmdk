#!/usr/bin/perl
#
# Copyright 2015-2017, Intel Corporation
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
# check_whitespace -- scrub source tree for whitespace errors
#

use strict;
use warnings;

use File::Basename;
use File::Find;

my $Me = $0;
$Me =~ s,.*/,,;

$SIG{HUP} = $SIG{INT} = $SIG{TERM} = $SIG{__DIE__} = sub {
	die @_ if $^S;

	my $errstr = shift;

	die "$Me: ERROR: $errstr";
};

my $Errcount = 0;

#
# err -- emit error, keep total error count
#
sub err {
	warn "$Me: ERROR: ", @_, "\n";
	$Errcount++;
}

#
# check_whitespace -- run the checks on the given file
#
sub check_whitespace {
	my ($full, $file) = @_;
	my $fh;

	open($fh, '<', $full) or die "$full $!\n";

	my $line = 0;
	my $eol;
	my $nf = 0;
	while (<$fh>) {
		$line++;
		$eol = /\n/s;
		if (/^\.nf$/) {
			err("$full: $line: nested .nf") if $nf;
			$nf = 1;
		} elsif (/^\.fi$/) {
			$nf = 0;
		} elsif ($nf == 0) {
			chomp;
			err("$full: $line: trailing whitespace") if /\s$/;
			err("$full: $line: spaces before tabs") if / \t/;
		}
	}

	close($fh);

	err("$full: $line: .nf without .fi") if $nf;
	err("$full: noeol") unless $eol;
}

sub check_whitespace_with_exc {
	my ($full) = @_;

	$_ = $full;

	return 0 if /^[.\/]*src\/jemalloc.*/;
	return 0 if /^[.\/]*src\/common\/queue\.h/;

	$_ = basename($full);

	return 0 unless /^(README.*|LICENSE.*|Makefile.*|.gitignore|TEST.*|RUNTESTS|check_whitespace|.*\.([chp13s]|sh|map|cpp|hpp|inc|PS1|ps1|md))$/;
	return 0 if -z;

	check_whitespace($full, $_);
	return 1;
}

my $verbose = 0;
my $force = 0;
my $recursive = 0;

sub check {
	my ($file) = @_;
	my $r;

	if ($force) {
		$r = check_whitespace($file, basename($file));
	} else {
		$r = check_whitespace_with_exc($file);
	}

	if ($verbose) {
		if ($r == 0) {
			printf("skipped $file\n");
		} else {
			printf("checked $file\n");
		}
	}
}

my @files = ();

foreach my $arg (@ARGV) {
	if ($arg eq '-v') {
		$verbose = 1;
		next;
	}
	if ($arg eq '-f') {
		$force = 1;
		next;
	}
	if ($arg eq '-r') {
		$recursive = 1;
		next;
	}
	if ($arg eq '-g') {
		@files = `git ls-tree -r --name-only HEAD`;
		chomp(@files);
		next;
	}
	if ($arg eq '-h') {
		printf "Options:
     -g - check all files tracked by git
     -r dir - recursively check all files in specified directory
     -v verbose - print whether file was checked or not
     -f force - disable blacklist\n";
		exit 1;
	}

	if ($recursive == 1) {
		find(sub {
			my $full = $File::Find::name;

			if (!$force &&
			   ($full eq './.git' ||
			    $full eq './src/jemalloc' ||
			    $full eq './src/debug' ||
			    $full eq './src/nondebug' ||
			    $full eq './rpmbuild' ||
			    $full eq './dpkgbuild')) {
				$File::Find::prune = 1;
				return;
			}

			return unless -f;

			push @files, $full;
		}, $arg);

		$recursive = 0;
		next;
	}

	push @files, $arg;
}

if (!@files) {
	printf "Empty file list!\n";
}

foreach (@files) {
	check($_);
}

exit $Errcount;
