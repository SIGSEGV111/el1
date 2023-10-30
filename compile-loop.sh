#!/bin/bash -eu
source "$HOME/bin/bash-commons/shell-util.sh"
parseCommandlineArguments "f:follow" "t:target=string?dev" "j:jobs:NUMCPUS=integer?1" -- "$@"

less_args=("--RAW-CONTROL-CHARS" "--clear-screen" "--tabs=4" "--ignore-case" "--SILENT")

if (($__follow)); then
	less_args+=("--exit-follow-on-close" "+F")
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
trap "" ERR
set +e +o pipefail +o errtrace

export GTEST_COLOR=yes
while true; do
	make -j $__jobs "$__target" 2>&1 | less "${less_args[@]}"
	sleep 0.5
done
