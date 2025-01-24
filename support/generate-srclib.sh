#!/bin/bash -eu
cd "$(dirname "$(readlink -f "$0")")/.."

readarray -t CPP_FILES < <(find src/el1 -maxdepth 1 -type f -iname "*.cpp" | sort | grep -v "/el1\.cpp$")
readarray -t HPP_FILES < <(find src/el1 -maxdepth 1 -type f -iname "*.hpp" | sort | grep -v "/el1\.hpp$")

rm -rf gen/srclib gen/srclib.tmp
mkdir -p gen/srclib.tmp/include/el1
cp --reflink=auto "${CPP_FILES[@]}" gen/srclib.tmp/include/el1/
cp --reflink=auto "${HPP_FILES[@]}" gen/srclib.tmp/include/el1/

(
	echo "#define EL_AMALGAM"
	for f in "${HPP_FILES[@]}"; do
		b="$(basename "$f")"
		echo "#include \"$b\""
	done
	for f in "${CPP_FILES[@]}"; do
		b="$(basename "$f")"
		echo "#include \"$b\""
	done
) > gen/srclib.tmp/include/el1/el1.cpp

(
	for f in "${HPP_FILES[@]}"; do
		b="$(basename "$f")"
		echo "#include \"$b\""
	done
) > gen/srclib.tmp/include/el1/el1.hpp

mv gen/srclib.tmp gen/srclib
