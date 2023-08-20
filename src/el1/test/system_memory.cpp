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

	TEST(system_memory, ProgramMap)
	{
		const progmap_t progmap = ProgramMap();

		EXPECT_LT(progmap.text.start, progmap.text.end);
		EXPECT_LT(progmap.data.start, progmap.data.end);
		EXPECT_LE(progmap.bss.start, progmap.bss.end);

		EXPECT_NE(progmap.text.start, nullptr);
		EXPECT_NE(progmap.text.end, nullptr);

		EXPECT_NE(progmap.data.start, nullptr);
		EXPECT_NE(progmap.data.end, nullptr);

		// fprintf(stderr, "progmap.text.start = %p\n", progmap.text.start);
		// fprintf(stderr, "progmap.text.end   = %p\n", progmap.text.end);
		// fprintf(stderr, "progmap.data.start = %p\n", progmap.data.start);
		// fprintf(stderr, "progmap.data.end   = %p\n", progmap.data.end);
		// fprintf(stderr, "progmap.bss.start  = %p\n", progmap.bss.start);
		// fprintf(stderr, "progmap.bss.end    = %p\n", progmap.bss.end);
  //
		// const auto lines = TFile("/proc/self/maps").Stream().Transform(TUTF8Decoder()).Transform(TLineReader()).Collect();
		// for(const TString& line : lines)
		// {
		// 	// auto fields = line.Split(' ', 6U);
		// 	// const auto addrs = fields[0].Split('-', 2U);
		// 	// const TString& str_start = addrs[0];
		// 	// const TString& str_end = addrs[1];
		// 	// const TString& str_prot = fields[1];
		// 	// const TString& str_file_offset = fields[2];
		// 	// const TString& str_dev = fields[3];
		// 	// const TString& str_file = fields[4];
  //
		// 	std::cerr<<line.MakeCStr().get()<<'\n';
		// 	if(line.Contains("[heap]"))
		// 		break;
		// }
  //
		// // TODO: parse /proc/self/maps and compare against progmap
		// // TODO: requires ability to parse hexadecimal numbers => io::text
	}

	TEST(system_memory, HeapMapping)
	{
		auto p = std::unique_ptr<int>(new int(0));
		auto s = HeapMapping();
		EXPECT_NE(s.start, nullptr);
		EXPECT_NE(s.end, nullptr);
		EXPECT_LT(s.start, s.end);
		EXPECT_GT(p.get(), s.start);
		#ifdef EL1_WITH_VALGRIND
		if(RUNNING_ON_VALGRIND == 0)
		#endif
		{
			EXPECT_LT(p.get(), s.end); // this test fails in valgrind
		}
		EXPECT_LT((usys_t)s.end - (usys_t)s.start, 512U * 1024U * 1024U);
	}
}
