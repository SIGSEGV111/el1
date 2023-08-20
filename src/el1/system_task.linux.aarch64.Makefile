gen/std/system_task.linux.aarch64.o: src/el1/system_task.linux.aarch64.cpp \
 src/el1/def.hpp
	mkdir -p gen/std
	'/bin/clang++' src/el1/system_task.linux.aarch64.cpp -o gen/std/system_task.linux.aarch64.o -c '-O0' '-g' '-flto' '-DEL1_WITH_POSTGRES' '-DEL1_WITH_VALGRIND' '-ftemplate-depth=50' '-fPIC' '-fdiagnostics-color=always' '-Wall' '-Wextra' '-Werror' '-Wno-unused-parameter' '-Wno-error=unused-function' '-I' 'submodules/googletest/googletest/include' '-std=gnu++20' '-Wno-unused-const-variable' '-I/usr/include/pgsql' '-DOPENSSL_LOAD_CONF' -I src
	./support/generate-cpp-makefile.sh src/el1/system_task.linux.aarch64.cpp
