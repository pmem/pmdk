#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation

#
# run-doc-update.sh - is called inside a Docker container to build docs in the current repository,
#		it checks if current branch is a 'valid' one (to only publish "merged" content, not from a PR),
#		and it creates a pull request with an update of our docs (on 'main' branch of pmem.github.io repo).
#
set -e

if [[ -z "${DOC_UPDATE_GITHUB_TOKEN}" ]]; then
	echo "ERROR: To build documentation and upload it as a Github pull request, " \
		"variable 'DOC_UPDATE_GITHUB_TOKEN' has to be provided."
	exit 1
fi

if [[ -z "${WORKDIR}" ]]; then
	echo "ERROR: The variable WORKDIR has to contain a path to the root " \
		"of this project."
	exit 1
fi

BOT_NAME="pmem-bot"
USER_NAME="pmem"
PAGES_REPO_NAME="pmem.github.io"

DOC_REPO_DIR=$(mktemp -d -t pmem_io-XXX)
ARTIFACTS_DIR=$(mktemp -d -t ARTIFACTS-XXX)

ORIGIN="https://${DOC_UPDATE_GITHUB_TOKEN}@github.com/${BOT_NAME}/${PAGES_REPO_NAME}"
UPSTREAM="https://github.com/${USER_NAME}/${PAGES_REPO_NAME}"

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

pushd ${WORKDIR}/doc
echo "Build docs and copy man & web md"
make -j$(nproc) web

mv ./web_linux ${ARTIFACTS_DIR}
mv ./generated/libs_map.yml ${ARTIFACTS_DIR}
popd

echo "Clone bot's pmem.io repo"
git clone --depth=1 ${ORIGIN} ${DOC_REPO_DIR}
pushd ${DOC_REPO_DIR}
git remote add upstream ${UPSTREAM}
git fetch upstream

git config --local user.name ${BOT_NAME}
git config --local user.email "${BOT_NAME}@intel.com"
hub config --global hub.protocol https

echo "Checkout new branch (based on 'main') for PR"
DOCS_BRANCH_NAME="pmdk-${TARGET_DOCS_DIR}-docs-update"
git checkout -B ${DOCS_BRANCH_NAME} upstream/main
git clean -dfx

echo "Copy content"
rsync -a ${ARTIFACTS_DIR}/web_linux/ ./content/pmdk/manpages/linux/${TARGET_DOCS_DIR}/ --delete

if [ ${TARGET_BRANCH} = "master" ]; then
	cp ${ARTIFACTS_DIR}/libs_map.yml data/
fi

echo "Add and push changes"
# git commit command may fail if there is nothing to commit.
# In that case we want to force push anyway (there might be open pull request
# with changes which were reverted).
git add -A
git commit -m "pmdk: automatic docs update for '${TARGET_BRANCH}'" && true
git push -f ${ORIGIN} ${DOCS_BRANCH_NAME}

echo "Make a Pull Request"
# When there is already an open PR or there are no changes an error is thrown, which we ignore.
GITHUB_TOKEN=${DOC_UPDATE_GITHUB_TOKEN} hub pull-request -f \
	-b ${USER_NAME}:main \
	-h ${BOT_NAME}:${DOCS_BRANCH_NAME} \
	-m "pmdk: automatic docs update for '${TARGET_BRANCH}'" && true

popd
rm -rf ${DOC_REPO_DIR}
rm -rf ${ARTIFACTS_DIR}

exit 0
