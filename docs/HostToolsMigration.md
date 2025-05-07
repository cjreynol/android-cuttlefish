# Notes about Github host tool development

TODO note that the Cuttlefish host tools are now Github-first development.
TODO make a note to use `aosp-main` until the Q2 android release for `aosp-latest-release` branch (is that date public?)
TODO link to the SAC page/announcement of `aosp-main` turndown

## developing the host tools

Much the same, except now (most of) the internal binaries are also built out of Github.  To use those instead of the ones from a fetched host package, either fetch with `--host_substitutions:all` or create the device with that flag.

TODO example invocations
TODO mention build instructions, even better point to them in another README doc
TODO mention symlinking to built `cvd` or using `bazel run //cuttlefish/package:cvd`

## Android Platform development

Now guest and host development must be done separately.
TODO note about Android Platform `bazel` causing failures.


## other points

TODO point to our CONTRIBUTING
TODO make sure our main README has sections for getting started (simplest flow to build and run), building, running, submitting PRs
TODO mention releases and version dev branches
