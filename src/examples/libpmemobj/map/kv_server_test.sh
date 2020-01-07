#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2016, Intel Corporation

set -euo pipefail

MAP=ctree
PORT=9100
POOL=$1

# start a new server instance
./kv_server $MAP $POOL $PORT &

# wait for the server to properly start
sleep 1

# insert a new key value pair and disconnect
RESP=`echo -e "INSERT foo bar\nGET foo\nBYE" | nc 127.0.0.1 $PORT`
echo $RESP

# remove previously inserted key value pair and shutdown the server
RESP=`echo -e "GET foo\nREMOVE foo\nGET foo\nKILL" | nc 127.0.0.1 $PORT`
echo $RESP
