#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# set-vars.sh - set required environment variables
#

set -e

export CI_FILE_PUSH_IMAGE_TO_REPO=/tmp/push_image_to_repo_flag
export CI_FILE_SKIP_BUILD_PKG_CHECK=/tmp/skip_build_package_check
