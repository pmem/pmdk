#
# Copyright (c) 2014-2015, Intel Corporation
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
#     * Neither the name of Intel Corporation nor the names of its
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
# pmempool.sh -- bash completion script for pmempool
#

_pmempool_gen()
{
	COMPREPLY=($(compgen -W "${1}" -- ${cur}))
}

_pmempool_get_cmds()
{
	echo -n $(pmempool --help | grep -e '^\S\+\s\+-' |\
			grep -o '^\S\+' | sed '/help/d')
}

_pmempool_get_opts()
{
	local c=$1
	local opts=$(pmempool ${c} --help | grep -o -e "-., --\S\+" |\
			grep -o -e "--\S\+")
	_pmempool_gen "${opts}"
}

_pmempool_get_values()
{
	local cmd=$1
	local opt=$2
	local vals=$(pmempool ${cmd} --help |\
			grep -o -e "${opt}\s\+\S\+|\S\+" |\
			sed "s/${opt}\s\+\(\S\+|\S\+\)/\1/" |\
			sed "s/|/ /")
	_pmempool_gen "${vals}"
}

_pmempool_get_cmd()
{
	local cmd=$1
	local cmds=$2

	[[ ${cmds} =~ ${cmd} ]] && echo -n ${cmd}
}

_pmempool()
{
	local cur prev opts
	COMPREPLY=()

	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	cmds=$(_pmempool_get_cmds)
	cmds_all="${cmds} help"
	opts_pool_types="blk log"
	cmd=$(_pmempool_get_cmd "${COMP_WORDS[1]}" "${cmds_all}")

	if [[ ${cur} == -* ]]
	then
		_pmempool_get_opts "${cmd}"
	elif [[ ${prev} == --* ]]
	then
		_pmempool_get_values ${cmd} ${prev}
	elif [[ ${cmd} == create ]]
	then
		case "${COMP_WORDS[@]}" in
			*blk*|*log*|*--inherit*)
				COMPREPLY=()
				;;
			*)
				_pmempool_gen "${opts_pool_types}"
				;;
		esac
	elif [[ ${prev} == help ]]
	then
		_pmempool_gen "${cmds}"
	elif [[ ${prev} == pmempool ]]
	then
		_pmempool_gen "${cmds_all}"
	fi
}

complete -o default -F _pmempool pmempool
