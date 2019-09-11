#!/usr/bin/env bash
#
# Copyright 2017-2019, Intel Corporation
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
# get_aliases.sh -- generate map of manuals functions and libraries
#
# usage: run from /pmdk/doc/generated location without parameters:
# ./../../utils/get_aliases.sh
#
# This script searches manpages from section 7 then
# takes all functions from each section using specified pattern
# and at the end to every function it assign real markdown file
# representation based on *.gz file content
#
# Generated libs_map.yml file is used on gh-pages
# to handle functions and their aliases
#


list=("$@")
man_child=("$@")

function search_aliases {
children=$1
for i in ${children[@]}
do
	if [ -e $i ]
	then
		echo "Man: $i"
		content=$(head -c 150 $i)
		if [[ "$content" == ".so "* ]] ;
		then
			content=$(basename ${content#".so"})
			i="${i%.*}"
			echo "  $i: $content" >> $map_file
		else
			r="${i%.*}"
			echo "  $r: $i" >> $map_file
		fi
	fi
done
}

function list_pages {
	parent="${1%.*}"
	list=("$@")
	man_child=("$@")

	if [ "$parent" == "libpmem" ]; then
		man_child=($(ls pmem_*.3))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmem2" ]; then
		man_child=($(ls pmem2_*.3))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmemblk" ]; then
		man_child=($(ls pmemblk_*.3))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmemlog" ]; then
		man_child=($(ls pmemlog_*.3))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmemobj" ]; then
		man_child=($(ls pmemobj_*.3))
		man_child+=($(ls pobj_*.3))
		man_child+=($(ls oid_*.3))
		man_child+=($(ls toid_*.3))
		man_child+=($(ls direct_*.3))
		man_child+=($(ls d_r*.3))
		man_child+=($(ls tx_*.3))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmempool" ]; then
		man_child=($(ls pmempool_*.3))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "librpmem" ]; then
		man_child=($(ls rpmem_*.3))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ ${#man_child[@]} -ne 0 ]
	then
		list=${man_child[@]}
		search_aliases "${list[@]}"
	fi
}

man7=($(ls *.7))

map_file=libs_map.yml
[ -e $map_file ] && rm $map_file
touch $map_file

for i in "${man7[@]}"
do
echo "Library: $i"
	list_pages $i
done
