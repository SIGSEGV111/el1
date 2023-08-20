gen/std/system_time_timer.o: src/el1/system_time_timer.cpp \
 src/el1/system_time_timer.hpp src/el1/system_time.hpp src/el1/def.hpp \
 src/el1/io_types.hpp src/el1/system_waitable.hpp \
 src/el1/system_handle.hpp
	mkdir -p gen/std
	'/bin/g++' src/el1/system_time_timer.cpp -o gen/std/system_time_timer.o -c '-DEL1_WITH_POSTGRES' '-DEL1_WITH_VALGRIND' '-ftemplate-depth=50' '-fPIC' '-fdiagnostics-color=always' '-Wall' '-Wextra' '-Werror' '-Wno-unused-parameter' '-Wno-error=unused-function' '-I' 'submodules/googletest/googletest/include' '-std=gnu++20' '-Wno-unused-const-variable' '-I/usr/include/pgsql' '-DOPENSSL_LOAD_CONF' -I src
	./support/generate-cpp-makefile.sh src/el1/system_time_timer.cpp
