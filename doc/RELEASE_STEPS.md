# PMDK release steps

This document contains all the steps required to make a new release of PMDK.

\#define $VERSION = current full version (e.g. 1.11.2); $VER = major+minor only version (e.g. 1.11)

Make a release locally:
- add an entry to ChangeLog, remember to change the day of the week in the release date
  - for major/minor releases mention compatibility with the previous release, if needed
- update reference to stable release in README.md (update `git checkout tags/$VERSION-1`)
- git rm GIT_VERSION
- echo $VERSION > VERSION
- git add VERSION
- git commit -a -S -m "common: $VERSION release"
- git tag -a -s -m "PMDK Version $VERSION" $VERSION

Make an optional package:
- git archive --prefix="pmdk-$VERSION/" -o pmdk-$VERSION.tar.gz $VERSION
- uncompress the created archive in a new directory and create the final package:
```
  $ cd pmdk-$VERSION
  $ make doc
  $ touch .skip-doc
  $ cd ..
  $ tar czf pmdk-$VERSION.tar.gz pmdk-$VERSION/ --owner=root --group=root
```
- verify the created archive (uncompress & build one last time)
- gpg --armor --detach-sign pmdk-$VERSION.tar.gz

Undo temporary release changes:
- git cherry-pick 1a620814f6affe0535441565007c352a67f995c0
- git rm VERSION
- git commit --reset-author

Publish changes:
- for major/minor release:
  - git push upstream HEAD:master $VERSION
  - create and push stable-$VER branch: git checkout -b stable-$VER
  - create PR from stable-$VER to master
- for patch release:
  - git push upstream HEAD:stable-$VER $VERSION
  - create PR from stable-$VER to next stable (or to master, if the release is from the last stable branch)

Publish package and make it official:
- go to [GitHub's releases tab](https://github.com/pmem/pmdk/releases/new):
  - tag version: $VERSION, release title: PMDK Version $VERSION, description: copy entry from ChangeLog
  - upload pmdk-$VERSION.tar.gz and pmdk-$VERSION.tar.gz.asc
- announce the release on pmem group, Slack, and any other channels (if needed)

Later, for major release:
- bump version of Docker images (utils/docker/images/set-images-version.sh) to $VER+1
- once pmem.github.io repository contains new documentation, add on this repo a new tag ($VER)
  in data/releases_linux.yml and data/releases_windows.yml
<!-- to be updated with Windows removal -->
- update library version in [vcpkg](https://github.com/microsoft/vcpkg/blob/master/ports/pmdk) - file an issue for their maintainers
