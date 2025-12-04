#!/usr/bin/env bash

# assumes current working directory is .../android-cuttlefish/tools/buildutils

# mark changelogs stable
debchange --check-dirname-level 0 --changelog base/debian/changelog --release --distribution stable
debchange --check-dirname-level 0 --changelog frontend/debian/changelog --release --distribution stable

# check in the typically .gitignored bazel lockfile
git add ../../base/cvd/MODULE.bazel.lock
