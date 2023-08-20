gen/std/dev_i2c_opt3001.o: src/el1/dev_i2c_opt3001.cpp \
 src/el1/dev_i2c_opt3001.hpp src/el1/dev_i2c.hpp src/el1/def.hpp \
 src/el1/io_types.hpp src/el1/io_stream.hpp src/el1/error.hpp \
 src/el1/util.hpp src/el1/system_waitable.hpp src/el1/system_handle.hpp \
 src/el1/system_time.hpp src/el1/dev_gpio.hpp src/el1/system_task.hpp \
 src/el1/io_collection_list.hpp src/el1/system_memory.hpp \
 src/el1/io_collection_map.hpp src/el1/io_file.hpp \
 src/el1/io_text_string.hpp src/el1/io_text_encoding.hpp \
 src/el1/io_text_encoding_utf8.hpp src/el1/io_text.hpp \
 src/el1/util_function.hpp
	mkdir -p gen/std
	'/bin/clang++' src/el1/dev_i2c_opt3001.cpp -o gen/std/dev_i2c_opt3001.o -c '-O0' '-g' '-flto' '-DEL1_WITH_POSTGRES' '-DEL1_WITH_VALGRIND' '-ftemplate-depth=50' '-fPIC' '-fdiagnostics-color=always' '-Wall' '-Wextra' '-Werror' '-Wno-unused-parameter' '-Wno-error=unused-function' '-I' 'submodules/googletest/googletest/include' '-std=gnu++20' '-Wno-unused-const-variable' '-I/usr/include/pgsql' '-DOPENSSL_LOAD_CONF' -I src
	./support/generate-cpp-makefile.sh src/el1/dev_i2c_opt3001.cpp
