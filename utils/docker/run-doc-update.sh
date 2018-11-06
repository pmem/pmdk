#!/usr/bin/env bash
#
# Copyright 2018, Intel Corporation
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

set -e

ORIGIN="https://${GITHUB_TOKEN}@github.com/WeronikaLewandowska/pmdk"
UPSTREAM="https://github.com/wlemkows/pmdk"
TARGET_BRANCH=${TRAVIS_BRANCH}

# Clone bot repo
git clone ${ORIGIN}
cd pmdk
git remote add upstream ${UPSTREAM}

git config --local user.name "WeronikaLewandowska"
git config --local user.email "taj5wero@gmail.com.com"

git checkout master
git remote update
git rebase upstream/master

make doc

# Build & PR groff
git add -A
git commit -m "doc: automatic master docs update" && true
git push -f ${ORIGIN} master

# Makes pull request.
# When there is already an open PR or there are no changes an error is thrown, which we ignore.
hub pull-request -f -b wlemkows:master -h WeronikaLewandowska:master -m "doc: automatic master docs update" && true

git clean -dfx

# Copy man & PR web md
cd  ./doc
make web
cd ..

#mkdir ../web_manpages
mv ./doc/web_linux ../
mv ./doc/web_windows ../

# Checkout gh-pages and copy docs
git checkout -fb gh-pages upstream/gh-pages
git clean -dfx

cp -r  ../web_linux/* ./manpages/linux/master/
cp -r  ../web_windows/* ./manpages/windows/master/

# Add and push changes.
# git commit command may fail if there is nothing to commit.
# In that case we want to force push anyway (there might be open pull request with
# changes which were reverted).
git add -A
git commit -m "doc: automatic gh-pages docs update" && true
git push -f ${ORIGIN} gh-pages

hub pull-request -f -b wlemkows:gh-pages -h WeronikaLewandowska:gh-pages -m "doc: automatic gh-pages docs update" && true

exit 0
