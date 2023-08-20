#!/bin/bash -eu

exec </dev/null
renice 10 $$ >/dev/null 2>&1 || true
ionice -c idle -p $$
declare -r MY_PATH="$(dirname "$(readlink -f "$0")")"
cd "$MY_PATH"

declare -r COLOR_RED="$(echo -e '\033[0;31m')"
declare -r COLOR_YELLOW="$(echo -e '\033[1;33m')"
declare -r COLOR_PURPLE="$(echo -e '\033[1;35m')"
declare -r COLOR_NONE="$(echo -e '\033[0m')"
declare -r NUM_CPUS="$(grep "^processor" /proc/cpuinfo | wc -l)"

export PGDATABASE=postgres
export PGHOST=localhost
export PGUSER=postgres
export PGPASSWORD=postgres

function trace()
{
	set -x
	time "$@"
	{ set +x; } 2>/dev/null
}

function on_exit()
{
	code=$?
	set +eux
	pkill -KILL -f el1-tests
	pkill -P $$ -TERM
	exit $code
}

trap on_exit EXIT SIGINT SIGHUP SIGQUIT SIGTERM

VALGRIND_OPTS=(
	--quiet
	--leak-check=full
	--show-reachable=no
	--track-origins=yes
	--num-callers=30
	--trace-children=no
	--track-fds=yes
	--error-exitcode=1
	--suppressions="$MY_PATH/valgrind.supp"
	--gen-suppressions=all
)

function RunValgrind()
{
	declare arch use_valgrind
	arch="$("$CXX" -dumpmachine)"
	case "$arch" in
		x86_64*|aarch64*)
			use_valgrind=1
			;;
		arm*)
			use_valgrind=0
			;;
		*)
			echo "unknown architecture ${arch@Q}"
			use_valgrind=1
			;;
	esac

	if (($use_valgrind)); then
		if ! valgrind --log-file=valgrind.log "${VALGRIND_OPTS[@]}" "$@"; then
			cat valgrind.log
			return 1
		fi
	else
		"$@"
	fi
}

export LANG=C
CXX="${CXX:-g++}"

if "$CXX" --version | grep -qF "clang"; then
	echo "detected compiler: clang"
	declare -r COMPILER="clang"

	LTO_OPTS=(
		-flto
	)

	COMPILE_OPTIONS=(
		-DEL1_WITH_POSTGRES
		-DEL1_WITH_VALGRIND
		-g
		-ftemplate-depth=50
		-fPIC
		-fdiagnostics-color=always
		-Wall
		-Wextra
		-Werror
		-Wno-unused-parameter
		-Wno-error=unused-function
		-O0
		-I "submodules/googletest/googletest/include"
		-I "src"
		-std=gnu++20
		-flto
		$(pkg-config --cflags libpq)
	)

	LINKER_OPTIONS=(
		-lstdc++
		-lm
		$(pkg-config --libs libpq)
	)
else
	echo "detected compiler: gcc"
	declare -r COMPILER="gcc"

	LTO_OPTS=(
		-flto=$NUM_CPUS
	)

	COMPILE_OPTIONS=(
		-DEL1_WITH_POSTGRES
		-DEL1_WITH_VALGRIND
		-g
		-ftemplate-depth=50
		-fPIC
		-fdiagnostics-color=always
		-Wall
		-Wextra
		-Werror
		-Wno-unused-parameter
		-Wno-error=unused-function
		-O0
		-I "submodules/googletest/googletest/include"
		-I "src"
		-std=gnu++20
		-flto=$NUM_CPUS
		$(pkg-config --cflags libpq)
	)

	LINKER_OPTIONS=(
		$(pkg-config --libs libpq)
	)
fi


LCOV_STEPS=(dynlib)
LCOV_OPTIONS=(
	--coverage
)

ASAN_OPTIONS=(
)
# 	-fsanitize=address
# 	-fsanitize=leak
# 	-fsanitize=undefined

LIB_OPTIONS=("${COMPILE_OPTIONS[@]}" "${LINKER_OPTIONS[@]}")
LIB_OPTIONS+=(
	-shared
)

EXE_OPTIONS=("${COMPILE_OPTIONS[@]}" "${LINKER_OPTIONS[@]}")
EXE_OPTIONS+=(
	-pthread
)

echo -n "preparing gen directory ... "
rm -rf gen/dbg/amalgam gen/dbg/dynlib gen/dbg/optimized gen/testdata
mkdir -p gen/dbg/amalgam gen/dbg/dynlib gen/dbg/optimized gen/gtest gen/testdata
echo "done"
echo "compiling googletest ... "
(
	if ! [[ -f gen/gtest/lib/libgtest.a ]]; then
		cd gen/gtest
		cmake ../../submodules/googletest
		make
	fi
) &

# generate testdata
(
	cd gen/testdata
	mkdir dir
	mkfifo fifo
	touch empty-file
	echo "hello world" > hw.txt
	ln -s hw.txt good-symlink
	ln -s nx-file dead-symlink
	ln -s dir dir-symlink
	ln hw.txt hardlink.txt
	nc -lkU socket & pid=$!
	sleep 0.1
	kill -KILL $pid
)  >/dev/null 2>&1 &

echo -n "searching code files ... "
readarray -t CPP_FILES < <(find src/el1 -maxdepth 1 -type f -iname "*.cpp" | sort)
readarray -t HPP_FILES < <(find src/el1 -maxdepth 1 -type f -iname "*.hpp" | sort)
readarray -t TEST_FILES < <(find src/el1/test -type f -iname "*.cpp" | sort)

mkdir -p gen/dbg/amalgam/include/el1
(
	echo "#pragma once"
	for f in "${HPP_FILES[@]}"; do
		ln -snf "../../../../../$f" "gen/dbg/amalgam/include/el1/"
		b="$(basename "$f")"
		echo "#include \"$b\""
	done
) > gen/dbg/amalgam/include/el1/el1.hpp

(
	echo "#define EL_AMALGAM"
	echo "#include \"el1.hpp\""
	for f in "${CPP_FILES[@]}"; do
		ln -snf "../../../../../$f" "gen/dbg/amalgam/include/el1/"
		b="$(basename "$f")"
		echo "#include \"$b\""
	done
) > gen/dbg/amalgam/include/el1/el1.cpp

echo "done"

wait
echo "done compiling googletest"

readarray -t INCOMPLETE_FILES < <(grep --extended-regexp --files-with-matches "EL_NOT_IMPLEMENTED|TNotImplementedException|TODO" "${CPP_FILES[@]}" "${HPP_FILES[@]}" | grep -vF "src/el1/error.hpp" | sed 's|src/el1/||g' | sort)
if ((${#INCOMPLETE_FILES[@]} > 0)); then
	echo "$COLOR_YELLOW*** EL_NOT_IMPLEMENTED/TNotImplementedException/TODO found in: ${INCOMPLETE_FILES[@]}$COLOR_NONE"
fi

readarray -t FIXME_FILES < <(grep --extended-regexp --files-with-matches "FIXME" "${CPP_FILES[@]}" "${HPP_FILES[@]}" | grep -vF "src/el1/error.hpp" | sed 's|src/el1/||g' | sort)
if ((${#FIXME_FILES[@]} > 0)); then
	echo "$COLOR_PURPLE*** FIXME found in: ${FIXME_FILES[@]}$COLOR_NONE"
fi

echo "compiling el1 ..."

set +e
"$CXX" gen/dbg/amalgam/include/el1/el1.cpp "${TEST_FILES[@]}" gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a -o gen/dbg/amalgam/el1-tests "${EXE_OPTIONS[@]}" "${ASAN_OPTIONS[@]}" > "gen/dbg/amalgam/el1-tests.log" 2>&1 & compile_amalgam_pid=$!

nice -n20 "$CXX" gen/dbg/amalgam/include/el1/el1.cpp "${TEST_FILES[@]}" gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a -o gen/dbg/optimized/el1-tests "${EXE_OPTIONS[@]}" "${ASAN_OPTIONS[@]}" -O3 "${LTO_OPTS[@]}" > "gen/dbg/optimized/el1-tests.log" 2>&1 & compile_optimized_pid=$!

(
	OBJ_FILES=()
	for f in "${CPP_FILES[@]}"; do
		bn="$(basename "$f")"
		OBJ_FILES+=("gen/dbg/dynlib/$bn.o")
		"$CXX" -c "$f" -o "gen/dbg/dynlib/$bn.o" "${COMPILE_OPTIONS[@]}" "${LCOV_OPTIONS[@]}" > "gen/dbg/dynlib/$bn.o.log" 2>&1 &
	done
	wait

	"$CXX" "${OBJ_FILES[@]}" -o gen/dbg/dynlib/libel1.so "${LIB_OPTIONS[@]}" "${LCOV_OPTIONS[@]}" > "gen/dbg/dynlib/libel1.log" 2>&1
	"$CXX" "${TEST_FILES[@]}" gen/gtest/lib/libgtest.a gen/gtest/lib/libgtest_main.a -o gen/dbg/dynlib/el1-tests "${EXE_OPTIONS[@]}" "${LCOV_OPTIONS[@]}" -l el1 -L gen/dbg/dynlib > "gen/dbg/dynlib/el1-tests.log" 2>&1
)

wait $compile_amalgam_pid
echo "done compiling el1"

LOG_FILES=(
	"gen/dbg/dynlib/"*.o.log
	"gen/dbg/dynlib/libel1.log"
	"gen/dbg/amalgam/el1-tests.log"
)

for logfile in "${LOG_FILES[@]}"; do
	lines="$(wc -l < "$logfile")"
	if (($lines > 0)); then
		echo "$logfile:"
		echo "-------------------------------------------"
		cat "$logfile"
		echo
	fi
done

echo -n "preparing lcov ... "
(
	for step in "${LCOV_STEPS[@]}"; do
		mkdir ./gen/dbg/$step/coverage

		# generate baseline
		lcov --capture --initial --directory ./gen/dbg/$step --output-file ./gen/dbg/$step/coverage/lcov-baseline.info > ./gen/dbg/$step/coverage/lcov-baseline.log 2>&1 &
	done
	wait
)
echo "done"
echo "executing tests ..."

set +e
set -o pipefail
(
	cd gen/dbg/amalgam
	exec ./el1-tests --gtest_color=yes --gtest_print_time=0 --gtest_output=json:el1-tests.json 2>&1 | tee run.log
) & run_amalgam_pid=$!

(
	cd gen/dbg/dynlib
	LD_LIBRARY_PATH="$PWD" RunValgrind ./el1-tests --gtest_color=yes --gtest_print_time=0 --gtest_output=json:el1-tests.json > run.log 2>&1
)
test_dynlib_status=$?

wait $run_amalgam_pid
test_amalgam_status=$?
set -e

if (($test_amalgam_status == 0)) && ! cmp ./gen/dbg/amalgam/run.log ./gen/dbg/dynlib/run.log 2>/dev/null; then
	echo "*** RESULTS FROM DYNLIB ***"
	git diff --no-index --word-diff --minimal --color=always -- ./gen/dbg/amalgam/run.log ./gen/dbg/dynlib/run.log
fi


echo
echo "done executing tests"

echo -n "generating lcov report ... "
(
	for step in "${LCOV_STEPS[@]}" ; do
		(
			# capture counts from tests
			lcov --rc lcov_branch_coverage=1 --capture --directory ./gen/dbg/$step --output-file ./gen/dbg/$step/coverage/lcov-test.info > ./gen/dbg/$step/coverage/lcov-test.log 2>&1

			# combine baseline with counts
			lcov --rc lcov_branch_coverage=1 --add-tracefile ./gen/dbg/$step/coverage/lcov-baseline.info --add-tracefile ./gen/dbg/$step/coverage/lcov-test.info --output-file ./gen/dbg/$step/coverage/lcov-combined.info > ./gen/dbg/$step/coverage/lcov-combine.log 2>&1

			# filter unwanted stuff
			lcov --rc lcov_branch_coverage=1 --remove ./gen/dbg/$step/coverage/lcov-combined.info '/usr/include/*' '/usr/lib/*' '*/googletest/*' '*/src/el1/test/*' --output-file ./gen/dbg/$step/coverage/lcov.info > ./gen/dbg/$step/coverage/lcov-filter.log 2>&1

			# generate html
			mkdir ./gen/dbg/$step/coverage/html
			genhtml --quiet --output-directory ./gen/dbg/$step/coverage/html --title el1-$step --prefix "$(readlink -f ./src)" --num-spaces 4 --legend --function-coverage --branch-coverage --demangle-cpp ./gen/dbg/$step/coverage/lcov.info &

			# extract branch coverage
			grep -E "^BRDA:" gen/dbg/$step/coverage/lcov.info | wc -l > gen/dbg/$step/coverage/branches-total.count &
			grep -E "^BRDA:.*,(-|0)$" gen/dbg/$step/coverage/lcov.info | wc -l > gen/dbg/$step/coverage/branches-not-covered.count &

			wait
		) &
	done
	wait
)
echo "done"

for step in "${LCOV_STEPS[@]}" ; do
	echo "coverage el1-$step: file://$(readlink -f ./gen/dbg/$step/coverage/html/index.html)"
done

echo -n "waiting for optimized compilation ... "
set +e
wait $compile_optimized_pid
compile_optimized_status=$?
set -e
if (($compile_optimized_status != 0)); then
	echo "error"
	echo
	cat ./gen/dbg/optimized/el1-tests.log
	echo
else
	echo "ok"
fi

echo
if (($test_amalgam_status == 0 && $test_dynlib_status == 0 && compile_optimized_status == 0)); then
	echo "*** ALL OK ***"
	echo
	exit 0
else
	echo "*** FAILURE ***"
	echo
	exit 1
fi
