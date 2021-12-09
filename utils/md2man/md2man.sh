#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation
#

#
# md2man.sh -- convert markdown to groff man pages
#
# usage: md2man.sh file template outfile
#
# This script converts markdown file into groff man page using pandoc.
# It performs some pre- and post-processing for better results:
# - parse input file for YAML metadata block and read man page title,
#   section and version
# - cut-off metadata block and license
# - unindent code blocks
#

set -e
set -o pipefail

filename=$1
template=$2
outfile=$3
version=$4
title=`sed -n 's/^title:\ _MP(*\([A-Za-z_-]*\).*$/\1/p' $filename`
section=`sed -n 's/^title:.*\([0-9]\))$/\1/p' $filename`
secondary_title=`sed -n 's/^secondary_title:\ *\(.*\)$/\1/p' $filename`

dt="$(date --utc --date="@${SOURCE_DATE_EPOCH:-$(date +%s)}" +%F)"
# since generated docs are not kept in the repo the output dir may not exist
out_dir=`echo $outfile | sed 's/\(.*\)\/.*/\1/'`
mkdir -p $out_dir

cat $filename | sed -n -e '/# NAME #/,$p' |\
	pandoc -s -t man -o $outfile --template=$template \
	-V title=$title -V section=$section \
	-V date="$dt" -V version="$version" \
	-V year=$(date +"%Y") -V secondary_title="$secondary_title" |
sed '/^\.IP/{
N
/\n\.nf/{
	s/IP/PP/
    }
}'
