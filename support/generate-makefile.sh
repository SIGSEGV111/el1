#!/bin/bash -eu
cd "$(dirname "$(readlink -f "$0")")/.."
source "./support/common.sh"

for cpp_file in "${CPP_FILES[@]}"; do
	./support/generate-cpp-makefile.sh "$cpp_file" &
done

cat > Makefile <<EOF
.PHONY: release dev libs clean verify Makefile

release: gen/amalgam/libel1.so gen/amalgam/libel1.a gen/srclib/include/el1/el1.cpp

libs: gen/std/libel1.so gen/std/libel1.a gen/amalgam/libel1.so gen/amalgam/libel1.a gen/srclib/include/el1/el1.cpp

dev: gen/srclib/link-test

gen/std/libel1.so: ${OBJ_FILES[@]} gen/std/include
	mkdir -p gen/std
	${CXX@Q} ${OBJ_FILES[@]} -o gen/std/libel1.so ${LIB_OPTIONS[@]@Q} -I gen/std/include

gen/std/libel1.a: ${OBJ_FILES[@]} gen/std/include
	mkdir -p gen/std
	ar rcs gen/std/libel1.a ${OBJ_FILES[@]}

gen/std/link-test-so: gen/std/libel1.so gen/std/include
	${CXX@Q} support/link-test.cpp -o gen/std/link-test-so ${EXE_OPTIONS[@]@Q} -L gen/std -l el1 -I gen/std/include

gen/std/link-test-a: gen/std/libel1.a gen/std/include
	${CXX@Q} support/link-test.cpp gen/std/libel1.a -o gen/std/link-test-a ${EXE_OPTIONS[@]@Q} -I gen/std/include

gen/amalgam/el1.o: gen/srclib/include/el1/el1.cpp
	mkdir -p gen/amalgam
	${CXX@Q} gen/srclib/include/el1/el1.cpp -o gen/amalgam/el1.o -c ${COMPILE_OPTIONS[@]@Q} -I gen/srclib/include

gen/amalgam/libel1.so: gen/amalgam/el1.o gen/amalgam/include
	${CXX@Q} gen/amalgam/el1.o -o gen/amalgam/libel1.so ${LIB_OPTIONS[@]@Q}

gen/amalgam/libel1.a: gen/amalgam/el1.o gen/amalgam/include
	mkdir -p gen/amalgam
	ar rcs gen/amalgam/libel1.a gen/amalgam/el1.o

gen/amalgam/link-test-so: gen/amalgam/libel1.so
	${CXX@Q} support/link-test.cpp -o gen/amalgam/link-test-so ${EXE_OPTIONS[@]@Q} -L gen/amalgam -l el1 -I gen/amalgam/include

gen/amalgam/link-test-a: gen/amalgam/libel1.a
	${CXX@Q} support/link-test.cpp gen/amalgam/libel1.a -o gen/amalgam/link-test-a ${EXE_OPTIONS[@]@Q} -I gen/amalgam/include

gen/srclib/include/el1/el1.cpp: ${CPP_FILES[@]} ${HPP_FILES[@]} support/generate-srclib.sh
	./support/generate-srclib.sh

gen/srclib/link-test: gen/srclib/include/el1/el1.cpp
	${CXX@Q} support/link-test.cpp gen/srclib/include/el1/el1.cpp -o gen/srclib/link-test ${EXE_OPTIONS[@]@Q} -I gen/srclib/include

gen/std/include: src/el1 support/generate-include-dir.sh ${HPP_FILES[@]}
	./support/generate-include-dir.sh gen/std/include

gen/amalgam/include: src/el1 support/generate-include-dir.sh ${HPP_FILES[@]}
	./support/generate-include-dir.sh gen/amalgam/include

gen/testdata: support/generate-testdata.sh \$(wildcard testdata/*)
	./support/generate-testdata.sh

gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a &: submodules/googletest
	set -eu; export CXX=${CXX@Q}; export CC=${CC@Q}; mkdir -p gen/gtest; cd gen/gtest; cmake ../../submodules/googletest; make

gen/srclib/gtests: gen/srclib/include/el1/el1.cpp \$(wildcard src/el1/test/*.cpp) gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a
	${CXX@Q} src/el1/test/*.cpp gen/srclib/include/el1/el1.cpp gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a -o gen/srclib/gtests ${EXE_OPTIONS[@]@Q} -I gen/srclib/include

gen/std/gtests: gen/std/include gen/testdata gen/std/libel1.so \$(wildcard src/el1/test/*.cpp) gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a
	${CXX@Q} src/el1/test/*.cpp gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a -o gen/std/gtests ${EXE_OPTIONS[@]@Q} -I gen/std/include -L gen/std -l el1

gen/amalgam/gtests: gen/amalgam/include gen/testdata gen/amalgam/libel1.a \$(wildcard src/el1/test/*.cpp) gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a
	${CXX@Q} src/el1/test/*.cpp gen/amalgam/libel1.a gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a -o gen/amalgam/gtests ${EXE_OPTIONS[@]@Q} -I gen/amalgam/include

link-tests: gen/std/link-test-a gen/amalgam/link-test-a gen/std/link-test-so gen/amalgam/link-test-so
	./gen/std/link-test-a
	./gen/amalgam/link-test-a
	LD_LIBRARY_PATH=gen/std ./gen/std/link-test-so
	LD_LIBRARY_PATH=gen/amalgam ./gen/amalgam/link-test-so

unit-tests: gen/std/gtests gen/amalgam/gtests gen/srclib/gtests
	LD_LIBRARY_PATH=gen/std ./gen/std/gtests
	LD_LIBRARY_PATH=gen/amalgam ./gen/amalgam/gtests
	valgrind ${VALGRIND_OPTIONS[@]@Q} --log-file=gen/srclib/valgrind.log ./gen/srclib/gtests

verify: link-tests unit-tests

Makefile:
	./support/generate-makefile.sh

clean:
	rm --recursive --force -- gen

include $(printCppFiles .Makefile)
EOF
wait
