#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022, Intel Corporation

set -e

BOT_NAME="pmem-bot"
USER_NAME="pmem"
PMDK_REPO_NAME="pmdk"
PAGES_REPO_NAME="pmem.github.io"

PMDK_REPO="https://github.com/${USER_NAME}/${PMDK_REPO_NAME}"
PAGES_REPO="https://github.com/${USER_NAME}/${PAGES_REPO_NAME}"
BOT_REPO="https://${DOC_UPDATE_GITHUB_TOKEN}@github.com/${BOT_NAME}/${PAGES_REPO_NAME}"

# Only 'master' or 'stable-*' branches are valid; determine docs location dir on gh-pages branch
TARGET_BRANCH=${CI_BRANCH}
if [[ "${TARGET_BRANCH}" == "master" ]]; then
	TARGET_DOCS_DIR="master"
elif [[ ${TARGET_BRANCH} == stable-* ]]; then
	TARGET_DOCS_DIR=v$(echo ${TARGET_BRANCH} | cut -d"-" -f2 -s)
else
	echo "Skipping docs build, this script should be run only on master or stable-* branches."
	echo "TARGET_BRANCH is set to: \'${TARGET_BRANCH}\'."
	exit 0
fi
if [ -z "${TARGET_DOCS_DIR}" ]; then
	echo "ERROR: Target docs location for branch: ${TARGET_BRANCH} is not set."
	exit 1
fi
# Clone PMDK repo
git clone ${PMDK_REPO}
cd ${PMDK_REPO_NAME}

git remote update
git checkout -B ${TARGET_BRANCH} upstream/${TARGET_BRANCH}

# Copy man & PR web md
cd  ./doc
make -j$(nproc) web
cd ..

mv ./doc/web_linux ../
mv ./doc/web_windows ../
mv ./doc/generated/libs_map.yml ../
cd ..

# Clone bot Github Pages repo
git clone ${BOT_REPO}
cd ${PAGES_REPO_NAME}
git remote add upstream ${PAGES_REPO}

git config --local user.name ${BOT_NAME}
git config --local user.email "${BOT_NAME}@intel.com"

# Checkout 'main' branch from the Github Pages repo
GH_PAGES_NAME="gh-pages-for-${TARGET_BRANCH}"
git checkout -B $GH_PAGES_NAME upstream/main
git clean -dfx

rsync -a ../web_linux/ ./content/pmdk/manpages/linux/${TARGET_DOCS_DIR}/ --delete
rsync -a ../web_windows/ ./content/pmdk/manpages/windows/${TARGET_DOCS_DIR}/ --delete \
	--exclude='librpmem'	\
	--exclude='rpmemd' --exclude='pmreorder'	\
	--exclude='daxio'

if [ $TARGET_BRANCH = "master" ]; then
	cp ../libs_map.yml data/
fi

# Add and push changes.
# git commit command may fail if there is nothing to commit.
# In that case we want to force push anyway (there might be open pull request
# with changes which were reverted).
git add -A
git commit -m "doc: automatic gh-pages PMDK ${TARGET_BRANCH} docs update" && true
git push -f ${BOT_REPO} $GH_PAGES_NAME

GITHUB_TOKEN=${DOC_UPDATE_GITHUB_TOKEN} hub pull-request -f \
	-b ${USER_NAME}:main \
	-h ${BOT_NAME}:${GH_PAGES_NAME} \
	-m "doc: automatic gh-pages PMDK ${TARGET_BRANCH} docs update" && true

cd ..
rm -r ${PMDK_REPO_NAME}
rm -r ${PAGES_REPO_NAME}
rm -r ./web_linux
rm -r ./web_windows
rm ./libs_map.yml

exit 0
