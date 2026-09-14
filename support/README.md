# Build and test support files

This directory contains support files used by the current Makefile as well as a
small number of manually invoked test helpers. The former legacy Makefile/source
library generator has been removed; the root `Makefile` is authoritative.

## Files used by the build/test targets

### `generate-testdata.sh`

Creates the generated test fixture tree (regular files, links, FIFO and Unix
socket) below the output directory. It is invoked directly by the root Makefile
for the test builds.

### `lifetime-negative.cpp`

Negative compile-time test input used by the `lifetime-check` target. Successful
compilation of cases that are expected to fail is treated as a test failure.

### `valgrind.sup`

Valgrind suppression file used by the Valgrind-enabled test targets.

### `tls-test-cert.pem` and `tls-test-key.pem`

Test-only TLS certificate and private key used by networking/TLS tests. They are
fixtures, not production credentials.

## Manually invoked helper

### `start-test-support-containers.sh`

Starts a local PostgreSQL test instance in Podman and prints the `PG*`
environment variables required by the PostgreSQL tests. It is intentionally not
started automatically by the build, because doing so would create and expose a
container as a side effect of `make test`.

Run it explicitly when PostgreSQL integration tests need a local server:

```bash
./support/start-test-support-containers.sh
```
