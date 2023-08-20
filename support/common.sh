: ${MAKE_OPTIONS:=-O0 -g -flto}
: ${CXX:=$(which g++)}
: ${CC:=$(which gcc)}

function die()
{
	echo "ERROR: $1" 1>&2
	exit 1
}

export CXX CC
export PGDATABASE=postgres
export PGHOST=localhost
export PGUSER=postgres
export PGPASSWORD=postgres

if "$CXX" --version | grep -qF "clang"; then
	declare -r CXX_COMPILER="EL_COMPILER_CLANG"

	LINKER_OPTIONS=(
		-lstdc++
		-lm
		$(pkg-config --libs libpq)
		-fuse-ld=lld
	)
else
	declare -r CXX_COMPILER="EL_COMPILER_GCC"

	LINKER_OPTIONS=(
		$(pkg-config --libs libpq)
	)
fi

if "$CC" --version | grep -qF "clang"; then
	declare -r C_COMPILER="EL_COMPILER_CLANG"
else
	declare -r C_COMPILER="EL_COMPILER_GCC"
fi

if [[ "$C_COMPILER" != "$CXX_COMPILER" ]]; then
	die "CC=$C_COMPILER mixed with CXX=$CXX_COMPILER"
fi

COMPILE_OPTIONS=(
	$MAKE_OPTIONS
	-DEL1_WITH_POSTGRES
	-DEL1_WITH_VALGRIND
	-ftemplate-depth=50
	-fPIC
	-fdiagnostics-color=always
	-Wall
	-Wextra
	-Werror
	-Wno-unused-parameter
	-Wno-error=unused-function
	-I "submodules/googletest/googletest/include"
	-std=gnu++20
	-Wno-unused-const-variable
	$(pkg-config --cflags libpq)
)

LIB_OPTIONS=("${COMPILE_OPTIONS[@]}" "${LINKER_OPTIONS[@]}" -shared)
EXE_OPTIONS=("${COMPILE_OPTIONS[@]}" "${LINKER_OPTIONS[@]}" -pthread)

VALGRIND_OPTIONS=(
	--quiet
	--leak-check=full
	--show-reachable=no
	--track-origins=yes
	--num-callers=30
	--trace-children=no
	--track-fds=yes
	--error-exitcode=1
	--suppressions=valgrind.supp
	--gen-suppressions=all
)

readarray -t CPP_FILES < <(find src/el1 -maxdepth 1 -type f -iname "*.cpp" | grep -v "/el1\.cpp$" | sort)
readarray -t HPP_FILES < <(find src/el1 -maxdepth 1 -type f -iname "*.hpp" | grep -v "/el1\.hpp$" | sort)

declare -a OBJ_FILES
for cpp_file in "${CPP_FILES[@]}"; do
	bare="${cpp_file%.cpp}"
	base="$(basename "$bare")"
	OBJ_FILES+=("gen/std/$base.o")
done
readonly COMPILE_OPTIONS LINKER_OPTIONS LIB_OPTIONS EXE_OPTIONS CPP_FILES HPP_FILES OBJ_FILES VALGRIND_OPTIONS

function printCppFiles()
{
	for cpp_file in "${CPP_FILES[@]}"; do
		bare="${cpp_file%.cpp}"
		echo -n " $bare$1"
	done
	echo
}

function printHppFiles()
{
	for hpp_file in "${HPP_FILES[@]}"; do
		bare="${hpp_file%.hpp}"
		echo -n " $bare$1"
	done
	echo
}
