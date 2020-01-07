#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation
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
