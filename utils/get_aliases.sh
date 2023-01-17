#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2023, Intel Corporation
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
parent=$2
for i in ${children[@]}
do
	if [ -e ../$parent/$i ]
	then
		echo "Man: $i"
		content=$(head -c 150 ../$parent/$i)
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
		man_child=($(ls -1 ../libpmem | grep -e ".*\.3$"))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmem2" ]; then
		man_child=($(ls -1 ../libpmem2 | grep -e ".*\.3$"))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmemset" ]; then
		man_child=($(ls -1 ../libpmemset | grep -e ".*\.3$"))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmemblk" ]; then
		man_child=($(ls -1 ../libpmemblk | grep -e ".*\.3$"))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmemlog" ]; then
		man_child=($(ls -1 ../libpmemlog | grep -e ".*\.3$"))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmemobj" ]; then
		man_child=($(ls -1 ../libpmemobj | grep -e ".*\.3$"))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ "$parent" == "libpmempool" ]; then
		man_child=($(ls -1 ../libpmempool | grep -e ".*\.3$"))
		echo -n "- $parent: " >> $map_file
		echo "${man_child[@]}" >> $map_file
	fi

	if [ ${#man_child[@]} -ne 0 ]
	then
		list=${man_child[@]}
		search_aliases "${list[@]}" "$parent"
	fi
}

man7=($(ls -1 ../*/ | grep -e ".*\.7$"))

map_file=libs_map.yml
[ -e $map_file ] && rm $map_file
touch $map_file

for i in "${man7[@]}"
do
echo "Library: $i"
	list_pages $i
done
