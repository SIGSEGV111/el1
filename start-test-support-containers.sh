#!/bin/bash -eux
podman kill el1-postgres && sleep 5
podman rm el1-postgres && sleep 5
podman run --pull=always --detach --restart=no --name=el1-postgres --rm --env POSTGRES_PASSWORD=postgres --publish 127.0.0.1:5432:5432 docker.io/library/postgres:latest
