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

if ! command -v nc >/dev/null 2>&1
then
	echo "ERROR: nc is required to create the Unix socket test fixture" >&2
	exit 1
fi

nc -lkU socket >/dev/null 2>&1 &
pid=$!

for (( i = 0; i < 50 && ! -S socket; i++ ))
do
	sleep 0.01
done

kill -KILL "$pid" >/dev/null 2>&1 || true
wait "$pid" 2>/dev/null || true

if [[ ! -S socket ]]
then
	echo "ERROR: 'nc -lkU socket' did not create a Unix socket; use a netcat implementation with -U support" >&2
	exit 1
fi
