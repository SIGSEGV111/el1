gen/std/dev_i2c.o: src/el1/dev_i2c.cpp src/el1/dev_i2c.hpp src/el1/def.hpp \
 src/el1/io_types.hpp src/el1/io_stream.hpp src/el1/error.hpp \
 src/el1/util.hpp src/el1/system_waitable.hpp src/el1/system_handle.hpp \
 src/el1/system_time.hpp
	mkdir -p gen/std
	'/bin/g++' src/el1/dev_i2c.cpp -o gen/std/dev_i2c.o -c '-DEL1_WITH_POSTGRES' '-DEL1_WITH_VALGRIND' '-ftemplate-depth=50' '-fPIC' '-fdiagnostics-color=always' '-Wall' '-Wextra' '-Werror' '-Wno-unused-parameter' '-Wno-error=unused-function' '-I' 'submodules/googletest/googletest/include' '-std=gnu++20' '-Wno-unused-const-variable' '-I/usr/include/pgsql' '-DOPENSSL_LOAD_CONF' -I src
	./support/generate-cpp-makefile.sh src/el1/dev_i2c.cpp
