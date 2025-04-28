#!/bin/bash -eux
podman kill el1-postgres && sleep 5
podman rm el1-postgres && sleep 5
podman pull docker.io/library/postgres:latest || true
podman run --detach --restart=no --name=el1-postgres --rm --env POSTGRES_PASSWORD=postgres --publish 127.0.0.1:5432:5432 docker.io/library/postgres:latest

cat << EOF

run this in your shell:
--------------------------
export PGDATABASE=postgres
export PGHOST=127.0.0.1
export PGUSER=postgres
export PGPASSWORD=postgres
export PGREQUIRESSL=0

EOF
