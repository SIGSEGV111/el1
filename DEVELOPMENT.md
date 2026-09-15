# Development helpers

The root build is defined by `Makefile` and `submodules/std-make-lib/Makefile.common`.
Files listed here are intentionally kept even when they are not prerequisites of a
normal `make` target.

## `compile-loop.sh`

Interactive developer helper that repeatedly invokes a selected make target and
shows the output through `less`. It is intended to be run manually while editing.
It depends on `/opt/amp-bash-commons/shell-util.sh` for command-line parsing.

Example:

```bash
./compile-loop.sh --target=test --jobs=8
```

## Test selection

The test targets accept a GoogleTest-compatible filter via the `TEST_FILTER`
make variable:

```bash
make test TEST_FILTER='system_time.*'
make test-debug TEST_FILTER='system_time.TTime_*:system_logbook.*'
```

`GTEST_FILTER` can be used as an environment variable instead. This also makes
`compile-loop.sh --gtest_filter=...` work with the normal make test targets:

```bash
GTEST_FILTER='system_time.*' make test
./compile-loop.sh --target=test-debug --gtest_filter='system_time.*'
```

The filter uses the normal GoogleTest syntax, including `:` separated positive
patterns and `-` separated negative patterns. An explicit `TEST_FILTER` or
`GTEST_FILTER` takes precedence over the automatic exclusions controlled by the
`WITH_*_TESTS` make variables. Without an explicit filter, the existing
automatic exclusions remain unchanged.

## `el1-env.sh`

Shell helper for projects built directly against the current el1 source tree.
It must be sourced, not executed. It exports `EL1_INCLUDE_DIR`, `EL1_LIB_DIR`,
`ARCH`, and, when a built library is available, `EL1_VERSION`.

Example:

```bash
source ./el1-env.sh
```

For packaged/installed el1 builds these environment variables are normally not
required.

## Support files

Build and test support files under `support/` are documented in
[`support/README.md`](support/README.md).

## Reference material

Files under `doc/` that are not part of the build are documented in
[`doc/README.md`](doc/README.md).
