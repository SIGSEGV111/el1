#include <gtest/gtest.h>
#include <el1/io_collection_map.hpp>
#include <el1/io_text_string.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::io::collection::map;
	using namespace el1::io::text::string;

	static const int arr_insert[] = { 29358, 8468, 21699, 6478, 18692, 12198, 20946, 17416, 23548, 28055, 15714, 23292, 26880, 17284, 15383, 471, 27252, 24871, 18965, 28021, 358, 19925, 4609, 25571, 23733, 20982, 20018, 26595, 22809, 7596, 7167, 14808, 27976, 8135, 31114, 17612, 3202, 15016, 15799, 6356, 16359,4113, 16695, 17010, 27248, 26901, 22352, 553, 11058, 15503, 18191, 20813, 24677, 19397, 561, 14086, 11420, 15099, 10971, 25039, 18573, 6307, 24247, 13210, 14875, 3408, 12362, 26820, 12893, 28567, 5089, 18509, 21504, 25827, 5219, 23809, 18941, 16561, 30874, 10991, 16144, 17678, 15234, 27599, 16005, 6089, 15035, 22075, 15007, 11967, 19653, 15690, 5316, 10759, 15303, 15467, 31660, 13682, 1315, 21682 };
	static const unsigned n_insert = sizeof(arr_insert) / sizeof(arr_insert[0]);

	TEST(io_collection_map, TSortedMap_Construct)
	{
		TSortedMap<int, int> int_map;
		EXPECT_EQ(int_map.Items().Count(), 0U);

		TSortedMap<TString, TString> string_map;
		EXPECT_EQ(string_map.Items().Count(), 0U);
	}

	TEST(io_collection_map, TSortedMap_ListConstructor)
	{
		using map_t = TSortedMap<int, TString>;
		using pair_t = map_t::kv_pair_t;

		TList<pair_t> unsorted = {
			{3, U"three"},
			{1, U"one"},
			{2, U"two"}
		};
		const pair_t* const storage = unsorted.ItemPtr(0);
		map_t sorted(std::move(unsorted));

		ASSERT_EQ(sorted.Items().Count(), 3U);
		EXPECT_EQ(sorted.Items().ItemPtr(0), storage);
		EXPECT_EQ(sorted.Items()[0].key, 1);
		EXPECT_EQ(sorted.Items()[1].key, 2);
		EXPECT_EQ(sorted.Items()[2].key, 3);
		EXPECT_EQ(sorted[1], U"one");
		EXPECT_EQ(sorted[2], U"two");
		EXPECT_EQ(sorted[3], U"three");

		TList<pair_t> already_sorted = {
			{1, U"one"},
			{2, U"two"},
			{3, U"three"}
		};
		const pair_t* const sorted_storage = already_sorted.ItemPtr(0);
		map_t assumed_sorted(std::move(already_sorted), EInputOrder::ASSUME_SORTED);
		EXPECT_EQ(assumed_sorted.Items().ItemPtr(0), sorted_storage);
		EXPECT_EQ(assumed_sorted[1], U"one");
		EXPECT_EQ(assumed_sorted[2], U"two");
		EXPECT_EQ(assumed_sorted[3], U"three");

		TList<pair_t> duplicates = {
			{2, U"first"},
			{1, U"one"},
			{2, U"second"}
		};
		EXPECT_THROW((map_t(std::move(duplicates))), TKeyAlreadyExistsException<int>);

		map_t initialized = {
			{3, U"three"},
			{1, U"one"},
			{2, U"two"}
		};
		EXPECT_EQ(initialized[1], U"one");
		EXPECT_EQ(initialized[2], U"two");
		EXPECT_EQ(initialized[3], U"three");

		auto reverse = [](const int& a, const int& b) -> int { return StdSorter(b, a); };
		TList<pair_t> reverse_items = {
			{1, U"one"},
			{3, U"three"},
			{2, U"two"}
		};
		map_t reverse_sorted(std::move(reverse_items), EInputOrder::UNSORTED, reverse);
		EXPECT_EQ(reverse_sorted.Items()[0].key, 3);
		EXPECT_EQ(reverse_sorted.Items()[1].key, 2);
		EXPECT_EQ(reverse_sorted.Items()[2].key, 1);
		EXPECT_EQ(reverse_sorted[1], U"one");
		EXPECT_EQ(reverse_sorted[2], U"two");
		EXPECT_EQ(reverse_sorted[3], U"three");
	}

	TEST(io_collection_map, TSortedMap_UniquePtr)
	{
		TSortedMap<int, std::unique_ptr<int>> map;
		std::unique_ptr<int>& x = EL_ANNOTATE_ERROR(map.Add(10, el1::New<int>(15)), TException, U"test");
		EXPECT_TRUE(*x == 15);
		EXPECT_TRUE(*map[10] == 15);
	}

	TEST(io_collection_map, TSortedMap_Add)
	{
		{
			TSortedMap<int, TString> map;
			map.Add(100, U"100");
			map.Add(101, U"101");
			EXPECT_THROW(map.Add(100, U"100"), TKeyAlreadyExistsException<int>);
			EXPECT_EQ(map.Items().Count(), 2U);
		}

		{
			TSortedMap<TString, TString> map;
			map.Add(U"100", U"100");
			map.Add(U"101", U"101");
			EXPECT_THROW(map.Add(U"100", U"100"), TKeyAlreadyExistsException<TString>);
			EXPECT_EQ(map.Items().Count(), 2U);
		}

		{
			TSortedMap<int, int> map;

			for(unsigned i = 0; i < n_insert; i++)
			{
				EXPECT_EQ(map.Items().Count(), i);
				map.Add(arr_insert[i], arr_insert[i]);

				for(unsigned j = 0; j <= i; j++)
				{
					EXPECT_EQ(map[arr_insert[j]], arr_insert[j]);
				}
			}

			EXPECT_EQ(map.Items().Count(), n_insert);
		}
	}

	TEST(io_collection_map, TSortedMap_Set)
	{
		{
			TSortedMap<int, TString> map;

			map.Set(100, U"100");
			EXPECT_EQ(map.Items().Count(), 1U);
			EXPECT_EQ(map[100], U"100");

			map.Set(101, U"101");
			EXPECT_EQ(map.Items().Count(), 2U);
			EXPECT_EQ(map[100], U"100");
			EXPECT_EQ(map[101], U"101");

			map.Set(100, U"200");
			EXPECT_EQ(map[100], U"200");
			EXPECT_EQ(map[101], U"101");
			EXPECT_EQ(map.Items().Count(), 2U);
		}

		{
			TSortedMap<int, int> map;

			for(unsigned i = 0; i < n_insert; i++)
			{
				EXPECT_EQ(map.Items().Count(), i);
				map.Set(arr_insert[i], arr_insert[i]);

				for(unsigned j = 0; j <= i; j++)
				{
					EXPECT_EQ(map[arr_insert[j]], arr_insert[j]);
				}
			}

			for(unsigned i = 0; i < n_insert; i++)
			{
				map.Set(arr_insert[i], arr_insert[i] + 1);
				EXPECT_EQ(map.Items().Count(), n_insert);

				unsigned j = 0;
				for(; j <= i; j++)
				{
					EXPECT_EQ(map[arr_insert[j]], arr_insert[j] + 1);
				}

				for(; j < n_insert; j++)
				{
					EXPECT_EQ(map[arr_insert[j]], arr_insert[j]);
				}
			}

			EXPECT_EQ(map.Items().Count(), n_insert);
		}
	}

	TEST(io_collection_map, TSortedMap_Remove)
	{
		{
			TSortedMap<int, TString> map;
			map.Add(1, U"hello world");
			map.Add(2, U"What's your name?");
			map.Add(3, U"Nice to meet you");
			map.Add(4, U"Goodbye!");
			EXPECT_EQ(map.Items().Count(), 4U);
			EXPECT_EQ(map[3], U"Nice to meet you");
			EXPECT_NE(map.Get(3), nullptr);
			EXPECT_TRUE(map.Remove(3));
			EXPECT_EQ(map.Get(3), nullptr);
			EXPECT_FALSE(map.Remove(3));
		}
	}
}
