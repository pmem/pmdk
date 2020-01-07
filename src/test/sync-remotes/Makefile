# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Intel Corporation

#
# src/test/sync-remote/Makefile -- copy common files to remote nodes
#

DIR_SYNC=../.sync-dir

# sync remote nodes
sync-remotes:
	@./copy-to-remote-nodes.sh common $(DIR_SYNC)

.PHONY: sync-remotes
