#!/bin/bash -eu
source "./support/common.sh"

cpp_file="$1"
bare="${cpp_file%.cpp}"
base="$(basename "$bare")"
exec 1> "$bare.Makefile"
echo -n "gen/std/"
"$CXX" -MM "${COMPILE_OPTIONS[@]}" "$cpp_file"
echo "	mkdir -p gen/std"
echo "	${CXX@Q} $cpp_file" -o gen/std/$base.o -c ${COMPILE_OPTIONS[@]@Q} -I src
echo "	./support/generate-cpp-makefile.sh $cpp_file"
