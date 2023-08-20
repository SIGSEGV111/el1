#!/bin/bash -eu
cd "$(dirname "$(readlink -f "$0")")/.."

rm -rf gen/testdata
mkdir -p gen/testdata
cp --reflink=auto -- testdata/* gen/testdata/

cd gen/testdata
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
