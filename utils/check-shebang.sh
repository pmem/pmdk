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
# utils/check-shebang.sh -- interpreter directive check script
#
set -e

err_count=0

for file in $@ ; do
        [ ! -f $file ] && continue
	SHEBANG=`head -n1 $file | cut -d" " -f1`
	[ "${SHEBANG:0:2}" != "#!" ] && continue
	if [ "$SHEBANG" != "#!/usr/bin/env" -a $SHEBANG != "#!/bin/sh" ]; then
		INTERP=`echo $SHEBANG | rev | cut -d"/" -f1 | rev`
		echo "$file:1: error: invalid interpreter directive:" >&2
		echo "	(is: \"$SHEBANG\", should be: \"#!/usr/bin/env $INTERP\")" >&2
		((err_count+=1))
	fi
done

if [ "$err_count" == "0" ]; then
	echo "Interpreter directives are OK."
else
	echo "Found $err_count errors in interpreter directives!" >&2
	err_count=1
fi

exit $err_count
