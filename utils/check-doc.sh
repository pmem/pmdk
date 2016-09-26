#!/bin/bash -e
#
# Copyright 2016, Intel Corporation
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
# Used to check whether changes to the generated documentation directory
# are made by the authorised user.
#
# usage: ./check-doc.sh <directory>
#

directory=$1

allowed_user="Generic builder <nvml-bot@intel.com>"

# check if git is installed and the check is performed on a git repository
if ! git status ${directory} > /dev/null; then
	echo "SKIP: no git installed or not a git repository"
	exit 0
fi

# check for changes and the author of the last commit
# the bot makes one commit pull requests
base_branch=$(git show-branch --current 2>/dev/null | grep '\*' | grep -v "add-doc-check" | head -n1 | sed 's/.*\[\(.*\)\].*/\1/' | sed 's/[\^~].*//')
base_commit=$(diff -u <(git rev-list --first-parent HEAD) <(git rev-list --first-parent ${base_branch}) | sed -ne 's/^ //p' | head -1)
changes=$(git diff --name-only ${base_commit} ${directory} | wc -l)
last_author=$(git --no-pager show -s --format='%aN <%aE>')

# no changes made to the given directory
if [ "$changes" -eq 0 ]; then
	exit 0
fi

if [ "$last_author" != "$allowed_user" ]; then
	echo "FAIL: changes to ${directory} allowed only by \"${allowed_user}\""
	exit -1
fi
