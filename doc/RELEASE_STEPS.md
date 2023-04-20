# PMDK release steps

This document contains all the steps required to make a new release of PMDK.

After following steps 1-3 you should have 2 local commits. The first one is a new, tagged version
of PMDK repository. The second commit is required to restore a "default" state of the repository
(for more details, why is this required, please see section ["For curious readers"](#7-for-curious-readers)).

\#define $VERSION = new release full version (e.g. 1.11.2); $VER = new major+minor only version (e.g. 1.11)

## 1. Make a release locally

Before doing the final release it's recommended to prepare a pre-release version - a "release candidate"
(or "rc" in short). This requires adding, e.g., `-rc1` to the VERSION string. When all tests and checks
ends properly, you can follow up with the final release. If any fix is required, it should be included in
another rc package (e.g., `-rc2`).

To assure community the release is a valid package from PMDK maintainers, it's recommended to sign the release
commit and the tag (`-S`/`-s` parameters in commands below). If you require to generate a GPG key follow
[these steps](https://docs.github.com/en/authentication/managing-commit-signature-verification/generating-a-new-gpg-key).
After that you'd also have to add this new key to your GitHub account - please do steps in
[this guide](https://docs.github.com/en/authentication/managing-commit-signature-verification/telling-git-about-your-signing-key).

To do a release:
- add an entry to ChangeLog, remember to change the day of the week in the release date
  - for major/minor releases mention compatibility with the previous release, if needed
- update reference to stable release in README.md (update line `git checkout tags/$VERSION-1` to the new release $VERSION)
  - this step is not recommended for rc version
- git rm GIT_VERSION
- echo $VERSION > VERSION
- git add VERSION
- git commit -a -S -m "common: $VERSION release"
- git tag -a -s -m "PMDK Version $VERSION" $VERSION

## 2. Make a package

When preparing a release on GitHub website we attach a package with the source code
with generated manpages (to avoid pandoc requirement).

Steps to make a package:
- git archive --prefix="pmdk-$VERSION/" -o pmdk-$VERSION.tar.gz $VERSION
- uncompress the created archive in **a new directory**
  - cd <some_path> && mv <...>/pmdk-$VERSION.tar.gz .
  - tar -xvf pmdk-$VERSION.tar.gz
- make the final package
  ```
    $ cd pmdk-$VERSION
    $ make doc
    $ touch .skip-doc
    $ cd ..
    $ tar czf pmdk-$VERSION.tar.gz pmdk-$VERSION/ --owner=root --group=root
  ```
- verify the created archive (uncompress & build one last time in a clean directory)
- gpg --armor --detach-sign pmdk-$VERSION.tar.gz

## 3. Undo temporary release changes
- git cherry-pick a6e1bc12c544612b1bba2f7c719765a13ca64926
- git rm VERSION
- git commit --reset-author

## 4. Publish changes
- for major/minor release:
  - git push upstream HEAD:master $VERSION
  - create and push stable-$VER branch:
  ```
    git checkout -b stable-$VER
    git push upstream stable-$VER
  ```
- for patch release:
  - git push upstream HEAD:stable-$VER $VERSION
  - create PR from stable-$VER to next stable (or to master, if the release is from the last stable branch)

## 5. Publish package and make it official

- go to [GitHub's releases tab](https://github.com/pmem/pmdk/releases/new)
  - tag version: $VERSION, release title: PMDK Version $VERSION, description: copy entry from ChangeLog
  - upload pmdk-$VERSION.tar.gz and pmdk-$VERSION.tar.gz.asc
- announce the release on pmem group, Slack, and any other channels (if needed)
  - this step is not recommended for rc version

## 6. Later, for major/minor release
- on master branch, bump version of Docker images (utils/docker/images/set-images-version.sh) to $VER+1
- once pmem.github.io repository contains new documentation, add a new tag ("$VER") in files
  `data/releases_linux.yml` and `data/releases_windows.yml`, based on previous tags in these files
<!-- to be updated with Windows removal -->
- update library version in [vcpkg](https://github.com/microsoft/vcpkg/blob/master/ports/pmdk) - file an issue for their maintainers

## 7. For curious readers

### PMDK version
To understand why we need step 3. from the above instruction we'd have to understand how we establish
the version of PMDK. It depends on several factors, e.g.:
 - if PMDK repo was cloned or downloaded,
 - git command availability in the OS,
 - tags availability (e.g., in case of git shallow clone).

To see the ful algorithm on how to determine PMDK version, please see `utils/version.sh` script.

To understand what a GIT_VERSION file is, please see, e.g., the message of `b3f0821dc41c4d2067a07b1545f32501fb9bb25a` commit .

### Branch state after release
An example state of stable branch after one release candidate and a final release can be seen
in a [stable-1.12 branch history](https://github.com/pmem/pmdk/commits/stable-1.12).

