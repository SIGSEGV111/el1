#!/bin/bash -eu
cd "$(dirname "$(readlink -f "$0")")/.."

OUT_DIR="${1:-gen}"
TESTDATA_DIR="$OUT_DIR/testdata"

rm -rf -- "$TESTDATA_DIR"
mkdir -p -- "$TESTDATA_DIR"
cp --reflink=auto -- testdata/* "$TESTDATA_DIR/"

cd "$TESTDATA_DIR"
mkdir dir
mkfifo fifo
touch empty-file
echo "hello world" > hw.txt
ln -s hw.txt good-symlink
ln -s nx-file dead-symlink
ln -s dir dir-symlink
ln hw.txt hardlink.txt

set +e
(
	nc -lkU socket & pid=$!
	sleep 0.1
	kill -KILL $pid
) 2>/dev/null
exit 0
