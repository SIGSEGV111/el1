#!/bin/bash -eu
declare dst="$1"

rm -rf "$dst" "$dst.tmp"
mkdir -p "$dst.tmp/el1"
cp --reflink=auto src/el1/*.hpp "$dst.tmp/el1/"
mv "$dst.tmp" "$dst"
