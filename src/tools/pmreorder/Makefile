# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation
#
# Makefile -- Makefile for pmreorder
#

include ../Makefile.inc

FLAKE8 := $(shell flake8 --version 2>/dev/null)

cstyle:
ifdef FLAKE8
	flake8 .
else
	@echo "Flake8 not found. Python files check skipped."
endif
