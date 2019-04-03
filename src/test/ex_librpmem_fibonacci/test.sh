#!/bin/bash

scp -o BatchMode=yes -r -p ../../examples/librpmem/{manpage,basic,hello} localhost:/dev/shm/jmmichal.node0//test_ex_librpmem
