#!/usr/bin/env bash

# Source this file from a shell to build other projects against this el1 tree:
#
#   source ./el1-env.sh
#
# For a non-native target architecture, set ARCH first so it matches the el1
# build directory selected by the Makefile:
#
#   export ARCH=armv7hl
#   source ./el1-env.sh

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
	echo "error: this script must be sourced: source $0" >&2
	exit 2
fi

_el1_setup_environment()
{
	local script_path;
	local source_dir;
	local arch;
	local include_dir;
	local lib_dir;
	local soname;
	local version;
	local readelf_cmd;

	script_path="$(readlink -f -- "${BASH_SOURCE[0]}")" || return 2;
	source_dir="$(dirname -- "$script_path")";

	arch="${ARCH:-}";
	if [[ -z "$arch" ]] && command -v rpm >/dev/null 2>&1; then
		arch="$(rpm --eval '%{_target_cpu}' 2>/dev/null || true)";
		if [[ "$arch" == '%{_target_cpu}' ]]; then
			arch='';
		fi
	fi
	if [[ -z "$arch" ]]; then
		arch="$(uname -m)" || return 2;
	fi

	include_dir="$source_dir/gen/include";
	lib_dir="$source_dir/gen/$arch/release";

	if [[ ! -f "$include_dir/el1/el1.hpp" ]]; then
		echo "error: $include_dir/el1/el1.hpp does not exist" >&2;
		echo "error: build the public headers first: make -C '$source_dir' headers" >&2;
		return 2;
	fi

	if [[ ! -e "$lib_dir/libel1.so" ]]; then
		echo "error: $lib_dir/libel1.so does not exist" >&2;
		echo "error: build el1 first: make -C '$source_dir' ARCH='$arch' release" >&2;
		return 2;
	fi

	readelf_cmd='';
	if command -v readelf >/dev/null 2>&1; then
		readelf_cmd='readelf';
	elif command -v llvm-readelf >/dev/null 2>&1; then
		readelf_cmd='llvm-readelf';
	fi

	soname='';
	if [[ -n "$readelf_cmd" ]]; then
		soname="$($readelf_cmd -d "$lib_dir/libel1.so" 2>/dev/null | sed -n 's/.*SONAME.*\[\(libel1\.so\.[^]]*\)\].*/\1/p' | head -n1)";
	fi

	if [[ -z "$soname" && -L "$lib_dir/libel1.so" ]]; then
		soname="$(basename -- "$(readlink -- "$lib_dir/libel1.so")")";
	fi

	case "$soname" in
		libel1.so.*)
			version="${soname#libel1.so.}";
			;;
		*)
			echo "error: unable to determine el1 ABI version from $lib_dir/libel1.so" >&2;
			return 2;
			;;
	esac

	export ARCH="$arch";
	export EL1_INCLUDE_DIR="$include_dir";
	export EL1_LIB_DIR="$lib_dir";
	export EL1_VERSION="$version";
}

if _el1_setup_environment; then
	unset -f _el1_setup_environment;
else
	unset -f _el1_setup_environment;
	return 2;
fi
