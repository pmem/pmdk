# PMDK release steps

This document contains all the steps required to make a new release of PMDK.

After following steps 1-3 you should have 2 local commits. The first one is a new, tagged version
of the PMDK repository. The second commit is required to restore a "default" state of the repository
(for more details, on why this is required, please see section ["For curious readers"](#8-for-curious-readers)).

As a helper you can export these 2 variables in your bash - with proper version set:

```bash
export VERSION=2.0.1-rc1   # the full version of the new release; -rc1 included just as an example
export VER=2.0             # the major+minor only version
```

## 1. Make a release locally

Before doing the final release it's recommended to prepare a pre-release version - a "release candidate"
(or "rc" in short). This requires adding, e.g., `-rc1` to the VERSION string. When all tests and checks
end properly, you can follow up with the final release. If any fix is required, it should be included in
another rc package (e.g., `-rc2`).

To assure the community that the release is a valid package from PMDK maintainers, it's recommended to sign the release
commit and the tag (`-S`/`-s` parameters in commands below). If you require to generate a GPG key follow
[these steps](https://docs.github.com/en/authentication/managing-commit-signature-verification/generating-a-new-gpg-key).
After that you'd also have to add this new key to your GitHub account - please do the steps in
[this guide](https://docs.github.com/en/authentication/managing-commit-signature-verification/telling-git-about-your-signing-key).

To do a release:
- add an entry to ChangeLog, remember to change the day of the week in the release date
  - for major releases mention compatibility with the previous release, if needed
- update reference to stable release in [INSTALL.md](INSTALL.md) (update line `git checkout tags/$VERSION-1` to the new release $VERSION)
  - omit this step for an rc version

```bash
git rm GIT_VERSION
echo $VERSION > VERSION
git add VERSION
git commit -a -S -m "common: $VERSION release"
git tag -a -s -m "PMDK Version $VERSION" $VERSION
```

## 2. Make a package

When preparing a release on the GitHub website we attach a package with the source code
with generated manpages (to avoid the `pandoc` requirement).

Steps to make a package:
- create the archive

```bash
git archive --prefix="pmdk-$VERSION/" -o pmdk-$VERSION.tar.gz $VERSION
```

- uncompress the created archive in **a new directory**

```bash
cd <some_path> && mv <...>/pmdk-$VERSION.tar.gz .
tar -xvf pmdk-$VERSION.tar.gz
```

- make the final package

```bash
cd pmdk-$VERSION
make doc
touch .skip-doc
cd ..
tar czf pmdk-$VERSION.tar.gz pmdk-$VERSION/ --owner=root --group=root
```

- verify the created archive (uncompress & build one last time in a clean directory)
- prepare a detached signature of the package (it will be called pmdk-$VERSION.tar.gz.asc)

```bash
gpg --armor --detach-sign pmdk-$VERSION.tar.gz
```

## 3. Undo temporary release changes

```bash
git cherry-pick a6e1bc12c544612b1bba2f7c719765a13ca64926  # a hash commit containing generic undo, called "common: git versions"
git rm VERSION
git commit --reset-author
```

## 4. Publish changes

  ```bash
    # push the tag
    git push upstream $VERSION
  ```

- for a major/minor release:
  - create a stable-$VER branch on the upstream repository
  - create a pull request to the new stable-$VER branch
- for a patch release:
  - create a pull request to the appropriate stable branch

## 5. Publish package and make it official

- go to [GitHub's releases tab](https://github.com/pmem/pmdk/releases/new) and fill in the form:
  - tag version: $VERSION,
  - release title: PMDK Version $VERSION,
  - description: copy entry from the ChangeLog
  - upload prepared package (pmdk-$VERSION.tar.gz) and its detached signature (pmdk-$VERSION.tar.gz.asc) as an attachment

## 6. Announcement (only major/minor releases)

Announce the release on the:
- [pmem.io](https://pmem.io/announcements/)
- [Slack](https://pmem-io.slack.com)
- [Google group](https://groups.google.com/g/pmem )

## 7. Later, for major/minor release

- on the stable-$VER branch, bump the version of Docker images (`utils/docker/images/set-images-version.sh`) to $VER
- once the pmem.github.io repository contains new documentation (thanks to `utils/docker/run-doc-update.sh` script run in CI),
  add a new tag ("$VER") in file `data/releases_linux.yml` based on previous tags in this file.

## 8. For curious readers

### PMDK version
To understand why we need step 3. from the above instruction we'd have to understand how we establish
the version of PMDK. It depends on several factors, e.g.:
 - whether the PMDK repository was cloned or downloaded,
 - git command availability in the OS,
 - tags availability (e.g., in the case of git's shallow clone).

To see the full algorithm on how to determine the PMDK version, please see [utils/version.sh](../utils/version.sh) script.

To understand what the GIT_VERSION file is, please see Pull Request #3110.

### Branch state after release
An example state of a stable branch after one release candidate and a final release can be seen
in a [stable-1.12 branch history](https://github.com/pmem/pmdk/commits/stable-1.12).
