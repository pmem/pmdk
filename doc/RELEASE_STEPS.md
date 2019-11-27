# PMDK release steps

This document contains all the steps required to make a new release of PMDK.

Make a release locally:
- add an entry to ChangeLog, remember to change the day of the week in the release date
- update reference to stable release in README.md
- git rm GIT_VERSION
- echo $VERSION > VERSION
- git add VERSION
- git commit -a -S -m "common: $VERSION release"
- git tag -a -s -m "PMDK Version $VERSION" $VERSION

Undo temporary release changes:
- git cherry-pick 1a620814f6affe0535441565007c352a67f995c0
- git rm VERSION
- git commit --reset-author

Publish changes, for major release:
- git push upstream HEAD:master $VERSION
- create and push stable-$VER branch
- create PR from stable-$VER to master

Publish changes, for minor release:
- git push upstream HEAD:stable-$VER $VERSION
- create PR from stable-$VER to next stable (or master, if release is from last stable branch)

Make a package:
- git archive --prefix="pmdk-$VERSION/" -o pmdk-$VERSION.tar.gz $VERSION
- verify the created archive (uncompress & build one last time)
- gpg --armor --detach-sign pmdk-$VERSION.tar.gz

Publish package and make it official:
- go to https://github.com/pmem/pmdk/releases/new:
 - tag version: $VERSION, release title: PMDK Version $VERSION, description: copy entry from ChangeLog
 - upload pmdk-$VERSION.tar.gz & pmdk-$VERSION.tar.gz.asc
- announce the release on pmem group

Later, for major release:
- bump version of Docker images (build-travis.sh, build-local.sh, build-image.sh, push-image.sh, pull-or-rebuild-image.sh) to $VERSION+1
- add new branch to valid-branches.sh
- once gh-pages contains new documentation, add $VER section in _data/releases_linux.yml and _data/releases_windows.yml on gh-pages branch
