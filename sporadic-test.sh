#!/bin/bash -eu
exec </dev/null

trap on_exit EXIT

function on_exit()
{
	code=$?
	pkill -P $$
	exit $code
}

export LD_LIBRARY_PATH="$PWD/gen/dbg/dynlib"

(
	ok=0
	fail=0
	for ((i=0;i<1000;i++)); do
		if nice -n $(($RANDOM % 3)) ./gen/dbg/amalgam/el1-tests > "amalgam-$i.log" 2>&1; then
			echo "amalgam $i: ok"
			rm -f "amalgam-$i.log"
			ok=$(($ok+1))
		else
			echo "amalgam $i: failed"
			fail=$(($fail+1))
		fi
	done
	echo "ok=$ok; fail=$fail"
	if (($fail != 0)); then exit 1; else exit 0; fi
) &

(
	ok=0
	fail=0
	for ((i=0;i<1000;i++)); do
		if nice -n $(($RANDOM % 3)) ./gen/dbg/optimized/el1-tests > "optimized-$i.log" 2>&1; then
			echo "optimized $i: ok"
			rm -f "optimized-$i.log"
			ok=$(($ok+1))
		else
			echo "optimized $i: failed"
			fail=$(($fail+1))
		fi
	done
	echo "ok=$ok; fail=$fail"
	if (($fail != 0)); then exit 1; else exit 0; fi
) &

(
	ok=0
	fail=0
	for ((i=0;i<1000;i++)); do
		if nice -n $(($RANDOM % 3)) ./gen/dbg/dynlib/el1-tests > "dynlib-$i.log" 2>&1; then
			echo "dynlib $i: ok"
			rm -f "dynlib-$i.log"
			ok=$(($ok+1))
		else
			echo "dynlib $i: failed"
			fail=$(($fail+1))
		fi
	done
	echo "ok=$ok; fail=$fail"
	if (($fail != 0)); then exit 1; else exit 0; fi
) &

wait
