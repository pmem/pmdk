#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

set -e

source `dirname $0`/valid-branches.sh

BOT_NAME="pmem-bot"
USER_NAME="pmem"
REPO_NAME="pmdk"

ORIGIN="https://${GITHUB_TOKEN}@github.com/${BOT_NAME}/${REPO_NAME}"
UPSTREAM="https://github.com/${USER_NAME}/${REPO_NAME}"
# master or stable-* branch
TARGET_BRANCH=${CI_BRANCH}
VERSION=${TARGET_BRANCHES[$TARGET_BRANCH]}

if [ -z $VERSION ]; then
	echo "Target location for branch $TARGET_BRANCH is not defined."
	exit 1
fi

# Clone bot repo
git clone ${ORIGIN}
cd ${REPO_NAME}
git remote add upstream ${UPSTREAM}

git config --local user.name ${BOT_NAME}
git config --local user.email "pmem-bot@intel.com"

git remote update
git checkout -B ${TARGET_BRANCH} upstream/${TARGET_BRANCH}

# Copy man & PR web md
cd  ./doc
make -j$(nproc) web
cd ..

mv ./doc/web_linux ../
mv ./doc/web_windows ../
mv ./doc/generated/libs_map.yml ../

# Checkout gh-pages and copy docs
GH_PAGES_NAME="gh-pages-for-${TARGET_BRANCH}"
git checkout -B $GH_PAGES_NAME upstream/gh-pages
git clean -dfx

rsync -a ../web_linux/ ./manpages/linux/${VERSION}/
rsync -a ../web_windows/ ./manpages/windows/${VERSION}/ \
	--exclude='librpmem'	\
	--exclude='rpmemd' --exclude='pmreorder'	\
	--exclude='daxio'

rm -r ../web_linux
rm -r ../web_windows

if [ $TARGET_BRANCH = "master" ]; then
	[ ! -d _data ] && mkdir _data
	cp ../libs_map.yml _data
fi

# Add and push changes.
# git commit command may fail if there is nothing to commit.
# In that case we want to force push anyway (there might be open pull request with
# changes which were reverted).
git add -A
git commit -m "doc: automatic gh-pages docs update" && true
git push -f ${ORIGIN} $GH_PAGES_NAME

hub pull-request -f -b ${USER_NAME}:gh-pages -h ${BOT_NAME}:${GH_PAGES_NAME} -m "doc: automatic gh-pages docs update" && true

exit 0
