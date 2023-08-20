#include <gtest/gtest.h>
#include <el1/system_memory.hpp>
#include <el1/io_file.hpp>
#include <el1/io_text.hpp>
#include <el1/io_text_string.hpp>
#include <el1/io_text_encoding_utf8.hpp>
#include <el1/io_collection_list.hpp>
#include <stdio.h>

#ifdef EL1_WITH_VALGRIND
#include <valgrind/valgrind.h>
#endif

using namespace ::testing;

namespace
{
	using namespace el1::system::memory;
	using namespace el1::io::file;
	using namespace el1::io::text;
	using namespace el1::io::text::string;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::io::collection::list;
}
