#!/bin/bash -eu
source "$HOME/bin/bash-commons/shell-util.sh"
parseCommandlineArguments "follow" -- "$@"

less_args=("--RAW-CONTROL-CHARS" "--clear-screen" "--tabs=4" "--ignore-case" "--SILENT")

if (($__follow)); then
	less_args+=("+F")
else
	less_args+=("--quit-on-intr")
fi

function on_exit()
{
	code=$?
	set +eux
	pkill -KILL -f el1-tests
	pkill -P $$ -TERM
	exit $code
}

trap on_exit EXIT SIGINT SIGHUP SIGQUIT SIGTERM

set +e
while true; do
	./dev-compile.sh 2>&1 | less "${less_args[@]}"
	sleep 0.5
done
