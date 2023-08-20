#include <gtest/gtest.h>
#include <el1/io_collection_list.hpp>
#include <el1/io_text_string.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::io::collection::list;
	using namespace el1::io::text::string;
	using namespace el1::testing;

	TEST(io_collection_list, TList_Construct_InitList)
	{
		TList<unsigned> list = { 0, 1, 2 };

		EXPECT_EQ(3U, list.Count());
		for(unsigned i = 0; i < 3; i++)
			EXPECT_EQ(i, list[i]);
	}

	TEST(io_collection_list, TList_Construct_Copy)
	{
		TList<unsigned> list1 = { 0, 1, 2 };
		TList<unsigned> list2(list1);

		EXPECT_EQ(3U, list2.Count());
		for(unsigned i = 0; i < 3; i++)
			EXPECT_EQ(i, list2[i]);
	}

	TEST(io_collection_list, TList_Construct_Move)
	{
		TList<unsigned> list1 = { 0, 1, 2 };
		TList<unsigned> list2(std::move(list1));

		EXPECT_EQ(3U, list2.Count());
		for(unsigned i = 0; i < 3; i++)
			EXPECT_EQ(i, list2[i]);
	}

	TEST(io_collection_list, TList_ItemCopyFail)
	{
		// fail during copy-construct phase in Insert() (on first item)
		TDebugItem::Reset();
		{
			TList<TDebugItem> list;

			for(unsigned i = 0; i < 2; i++)
				list.Append(TDebugItem(i));

			EXPECT_EQ(2U, list.Count());

			const TDebugItem item(3);
			TDebugItem::n_copy_before_throw = 1;
			EXPECT_THROW(list.Append(item), const TDebugItem*);

			EXPECT_EQ(2U, list.Count());
		}
		EXPECT_TRUE(TDebugItem::Check());

		// fail during copy-construct phase in Insert() (but on second item)
		TDebugItem::Reset();
		{
			TList<TDebugItem> list;
			const TDebugItem arr[] = { 0, 1 };
			TDebugItem::n_copy_before_throw = 2;

			EXPECT_EQ(list.Count(), 0U);
			EXPECT_THROW(list.Append(arr, 2), const TDebugItem*);
			EXPECT_EQ(list.Count(), 1U);
		}
		EXPECT_TRUE(TDebugItem::Check());

		// fail during CopyConstruct()
		TDebugItem::Reset();
		{
			const TDebugItem arr[] = { 0, 1 };
			TDebugItem::n_copy_before_throw = 2;

			EXPECT_THROW(TList<TDebugItem>(arr, 2), const TDebugItem*);
		}
		EXPECT_TRUE(TDebugItem::Check());
	}

	TEST(io_collection_list, TList_Append)
	{
		TDebugItem::Reset();
		{
			TList<TDebugItem> list;
			for(unsigned i = 0; i < 10; i++)
				list.Append(TDebugItem(i));

			EXPECT_EQ(10U, list.Count());
			for(unsigned i = 0; i < 10; i++)
				EXPECT_EQ(i, list[i]);

			const TDebugItem arr[] = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
			list.Append(arr, 10);

			EXPECT_EQ(20U, list.Count());
			for(unsigned i = 0; i < 20; i++)
				EXPECT_EQ(i, list[i]);
		}
		EXPECT_TRUE(TDebugItem::Check());

		TDebugItem::Reset();
		{
			TList<TDebugItem> list1 = { 0, 1, 2 };
			EXPECT_EQ(3U, list1.Count());

			TList<TDebugItem> list2 = { 3, 4, 5 };

			list2.Insert(0, list1);
			EXPECT_EQ(6U, list2.Count());
			for(unsigned i = 0; i < 6; i++)
				EXPECT_EQ(i, list2[i]);

			list2.Append(list1);
			EXPECT_EQ(9U, list2.Count());
			for(unsigned i = 0; i < 6; i++)
				EXPECT_EQ(i, list2[i]);
			EXPECT_EQ(0, list2[6]);
			EXPECT_EQ(1, list2[7]);
			EXPECT_EQ(2, list2[8]);
		}
		EXPECT_TRUE(TDebugItem::Check());
	}

	TEST(io_collection_list, TList_Insert)
	{
		// TODO improve this test: "hello world".ReplaceAt(5,1," brave new "); // this failed and was not caught by this test
		TDebugItem::Reset();
		{
			TList<TDebugItem> list;
			for(unsigned i = 0; i < 10; i++)
				list.Insert(0, TDebugItem(9 - i));

			EXPECT_EQ(10U, list.Count());

			for(unsigned i = 0; i < 10; i++)
				EXPECT_EQ(i, list[i]);

			const TDebugItem arr[] = { 1001, 1002 };
			list.Insert(1, arr, 2);

			EXPECT_EQ(12U, list.Count());

			EXPECT_EQ(0,    list[0]);
			EXPECT_EQ(1001, list[1]);
			EXPECT_EQ(1002, list[2]);

			for(unsigned i = 3; i < 12; i++)
				EXPECT_EQ(i - 2, list[i]);
		}
		EXPECT_TRUE(TDebugItem::Check());
	}

	TEST(io_collection_list, TList_Remove)
	{
		TDebugItem::Reset();
		{
			TList<TDebugItem> list;
			for(unsigned i = 0; i < 10; i++)
				list.Append(TDebugItem(i));

			EXPECT_EQ(10U, list.Count());
			list.Remove(5, 2);
			EXPECT_EQ(8U, list.Count());

			EXPECT_EQ( 0, list[ 0]);
			EXPECT_EQ( 1, list[ 1]);
			EXPECT_EQ( 2, list[ 2]);
			EXPECT_EQ( 3, list[ 3]);
			EXPECT_EQ( 4, list[ 4]);
			EXPECT_EQ( 7, list[ 5]);
			EXPECT_EQ( 8, list[ 6]);
			EXPECT_EQ( 9, list[ 7]);

			list.Remove(0);
			EXPECT_EQ(7U, list.Count());

			EXPECT_EQ( 1, list[ 0]);
			EXPECT_EQ( 2, list[ 1]);
			EXPECT_EQ( 3, list[ 2]);
			EXPECT_EQ( 4, list[ 3]);
			EXPECT_EQ( 7, list[ 4]);
			EXPECT_EQ( 8, list[ 5]);
			EXPECT_EQ( 9, list[ 6]);

			list.Remove(-1);
			EXPECT_EQ(6U, list.Count());

			EXPECT_EQ( 1, list[ 0]);
			EXPECT_EQ( 2, list[ 1]);
			EXPECT_EQ( 3, list[ 2]);
			EXPECT_EQ( 4, list[ 3]);
			EXPECT_EQ( 7, list[ 4]);
			EXPECT_EQ( 8, list[ 5]);
		}
		EXPECT_TRUE(TDebugItem::Check());

		TDebugItem::Reset();
		{
			TList<TDebugItem> list;
			for(unsigned i = 0; i < 1000; i++)
				list.Append(TDebugItem(i));

			EXPECT_EQ(list.Count(), 1000U);
			for(unsigned i = 1; i < 1000; i++)
			{
				list.Remove(-2, 1);
				EXPECT_EQ(1000U - i, list.Count());
			}
			EXPECT_EQ(list.Count(), 1U);
			list.Remove(0, 1);
			EXPECT_EQ(list.Count(), 0U);
		}
		EXPECT_TRUE(TDebugItem::Check());
	}

	TEST(io_collection_list, TList_RemoveItem)
	{
		TDebugItem::Reset();
		{
			TList<TDebugItem> list = { 0, 1, 1, 2, 3, 4, 5, 6, 5, 5, 6, 7, 8, 9, 1 };

			EXPECT_EQ(list.Count(), 15U);
			EXPECT_EQ(list.RemoveItem(0, NEG1), 1U);
			EXPECT_EQ(list.Count(), 14U);
			EXPECT_EQ(list.RemoveItem(5, NEG1), 3U);
			EXPECT_EQ(list.Count(), 11U);
			EXPECT_EQ(list.RemoveItem(1, NEG1), 3U);
			EXPECT_EQ(list.Count(),  8U);
			EXPECT_EQ(list.RemoveItem(2, NEG1), 1U);
			EXPECT_EQ(list.Count(),  7U);
			EXPECT_EQ(list.RemoveItem(3, NEG1), 1U);
			EXPECT_EQ(list.Count(),  6U);
			EXPECT_EQ(list.RemoveItem(7, NEG1), 1U);
			EXPECT_EQ(list.Count(),  5U);
			EXPECT_EQ(list.RemoveItem(8, NEG1), 1U);
			EXPECT_EQ(list.Count(),  4U);
			EXPECT_EQ(list.RemoveItem(1, NEG1), 0U);
			EXPECT_EQ(list.Count(),  4U);
			EXPECT_EQ(list.RemoveItem(5, NEG1), 0U);
			EXPECT_EQ(list.Count(),  4U);
			EXPECT_EQ(list.RemoveItem(4, NEG1), 1U);
			EXPECT_EQ(list.Count(),  3U);
			EXPECT_EQ(list.RemoveItem(6, NEG1), 2U);
			EXPECT_EQ(list.Count(),  1U);
			EXPECT_EQ(list.RemoveItem(9, NEG1), 1U);
			EXPECT_EQ(list.Count(),  0U);
		}
		EXPECT_TRUE(TDebugItem::Check());

		TDebugItem::Reset();
		{
			TList<TDebugItem> list;

			for(usys_t i = 0; i < 1000; i++)
				list.Append(rand() % 128);

			while(list.Count() > 0)
				list.RemoveItem(rand() % 128, rand() % 8);

		}
		EXPECT_TRUE(TDebugItem::Check());
	}

	TEST(io_collection_list, TList_Cut)
	{
		TDebugItem::Reset();
		{
			TList<TDebugItem> list = { 999, 0, 999, 999 };

			EXPECT_EQ(4U, list.Count());
			list.Cut(1, 2);
			EXPECT_EQ(1U, list.Count());
			EXPECT_EQ(0, list[0]);

			EXPECT_THROW(list.Cut(1, 1), el1::error::TIndexOutOfBoundsException);
		}
		EXPECT_TRUE(TDebugItem::Check());
	}

	TEST(io_collection_list, TList_Reverse)
	{
		{
			TList<int> list = { 0, 1, 2, 3, 4, 5 };
			list.Reverse();
			EXPECT_EQ(list.Count(), 6U);
			EXPECT_EQ(list[0], 5);
			EXPECT_EQ(list[1], 4);
			EXPECT_EQ(list[2], 3);
			EXPECT_EQ(list[3], 2);
			EXPECT_EQ(list[4], 1);
			EXPECT_EQ(list[5], 0);
		}

		{
			TList<int> list = { 0, 1, 2, 3, 4 };
			list.Reverse();
			EXPECT_EQ(list.Count(), 5U);
			EXPECT_EQ(list[0], 4);
			EXPECT_EQ(list[1], 3);
			EXPECT_EQ(list[2], 2);
			EXPECT_EQ(list[3], 1);
			EXPECT_EQ(list[4], 0);
		}

		{
			TList<int> list = { 0, 1 };
			list.Reverse();
			EXPECT_EQ(list.Count(), 2U);
			EXPECT_EQ(list[0], 1);
			EXPECT_EQ(list[1], 0);
		}

		{
			TList<int> list = { 0 };
			list.Reverse();
			EXPECT_EQ(list.Count(), 1U);
			EXPECT_EQ(list[0], 0);
		}

		{
			TList<int> list;
			list.Reverse();
			EXPECT_EQ(list.Count(), 0U);
		}
	}

	TEST(io_collection_list, TList_Find)
	{
		const TList<int> list = { 999, 0, 999, 999, 100, 999, 999, 10, 999, 10 };

		{
			TList<range_t> ranges = list.Find(999, 4);
			EXPECT_EQ(3U, ranges.Count());

			if(ranges.Count() == 3)
			{
				EXPECT_EQ(0U, ranges[0].index);
				EXPECT_EQ(1U, ranges[0].count);

				EXPECT_EQ(2U, ranges[1].index);
				EXPECT_EQ(2U, ranges[1].count);

				EXPECT_EQ(5U, ranges[2].index);
				EXPECT_EQ(1U, ranges[2].count);
			}
		}

		{
			TList<range_t> ranges = list.Find(999);
			EXPECT_EQ(4U, ranges.Count());

			if(ranges.Count() == 4)
			{
				EXPECT_EQ(0U, ranges[0].index);
				EXPECT_EQ(1U, ranges[0].count);

				EXPECT_EQ(2U, ranges[1].index);
				EXPECT_EQ(2U, ranges[1].count);

				EXPECT_EQ(5U, ranges[2].index);
				EXPECT_EQ(2U, ranges[2].count);

				EXPECT_EQ(8U, ranges[3].index);
				EXPECT_EQ(1U, ranges[3].count);
			}
		}

		{
			TList<range_t> ranges = list.Find(999, 0);
			EXPECT_EQ(0U, ranges.Count());
		}
	}

	TEST(io_collection_list, TList_Contains)
	{
		TList<int> list = { 999, 0, 999, 999 };

		EXPECT_TRUE(list.Contains(0));
		EXPECT_TRUE(list.Contains(999));
		EXPECT_FALSE(list.Contains(100));
	}

	TEST(io_collection_list, TList_NoCopyItem)
	{
		TDebugItem::Reset();
		{
			TList< std::unique_ptr<TDebugItem> > list;
			list.MoveAppend(std::unique_ptr<TDebugItem>(new TDebugItem(1)));
			list.MoveAppend(std::unique_ptr<TDebugItem>(new TDebugItem(2)));
			list.MoveAppend(std::unique_ptr<TDebugItem>(new TDebugItem(3)));
			list.MoveAppend(std::unique_ptr<TDebugItem>(new TDebugItem(4)));
			list.MoveAppend(std::unique_ptr<TDebugItem>(new TDebugItem(5)));
			EXPECT_EQ(list.Count(), 5U);
		}
		EXPECT_TRUE(TDebugItem::Check());
	}

	TEST(io_collection_list, TList_BinarySearch)
	{
		{
			const TList<int> list = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  0); }),  0U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  1); }),  1U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  2); }),  2U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  3); }),  3U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  4); }),  4U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  5); }),  5U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  6); }),  6U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  7); }),  7U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  8); }),  8U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  9); }),  9U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, 10); }), NEG1);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, 10); }, true), 9U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, -1); }, true), 0U);
		}

		{
			const TList<int> list = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  0); }),  0U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  1); }),  1U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  2); }),  2U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  3); }),  3U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  4); }),  4U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  5); }),  5U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  6); }),  6U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  7); }),  7U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  8); }),  8U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, 10); }), NEG1);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, 10); }, true), 8U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, -1); }, true), 0U);
		}

		{
			const TList<int> list = { 0, 1, 2, 3, 4, 6, 7, 8 };

			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  0); }),  0U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  1); }),  1U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  2); }),  2U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  3); }),  3U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  4); }),  4U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  5); }), NEG1);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  6); }),  5U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  7); }),  6U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  8); }),  7U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  9); }), NEG1);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, 10); }), NEG1);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, 10); }, true), 7U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item, -1); }, true), 0U);
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  5); }, true), 5U);
		}

		{
			const TList<TString> list = {
				"abc",
				"hello world",
				"test 123",
				"z9999"
			};

			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "abc"        ); }),  0U);
			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "hello world"); }),  1U);
			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "test 123"   ); }),  2U);
			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "z9999"      ); }),  3U);
			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "abc123"     ); }), NEG1);
			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "abc"        ); }, true), 0U);
			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "help!"      ); }, true), 1U);
			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "abc1"       ); }, true), 0U);
			EXPECT_EQ(list.BinarySearch([](const TString& item) { return StdSorter<TString>(item, "z1"         ); }, true), 3U);
		}

		{
			TList<usys_t> list;
			EXPECT_EQ(list.BinarySearch([](const int& item) { return StdSorter(item,  9); }), NEG1);

			for(usys_t c = 0; c < 200; c++)
			{
				list.Append(c);
				EXPECT_EQ(list.Count(), c+1);

				for(usys_t i = 0; i < c+1; i++)
				{
					EXPECT_EQ(list.BinarySearch([&](const usys_t& item) { return StdSorter<usys_t>(item, i); }, false), i);
					EXPECT_EQ(list.BinarySearch([&](const usys_t& item) { return StdSorter<usys_t>(item, i); }, true),  i);
				}

				EXPECT_EQ(list.BinarySearch([](const usys_t& item) { return StdSorter<usys_t>(item, 1000U); }, false), NEG1);
				EXPECT_EQ(list.BinarySearch([](const usys_t& item) { return StdSorter<usys_t>(item, 1000U); }, true), c);
			}
		}
	}

	TEST(io_collection_list, TList_Assign)
	{
		TDebugItem::Reset();
		{
			TList<TDebugItem> list1 = { 999, 0, 999, 999, 1, 2, 3 };
			TList<TDebugItem> list2 = { 0 };
			list2 = std::move(list1);
			EXPECT_EQ(list1.Count(), 0U);
			EXPECT_EQ(list2.Count(), 7U);
			EXPECT_EQ(list2[0], 999);
			EXPECT_EQ(list2[1],   0);
			EXPECT_EQ(list2[2], 999);
			EXPECT_EQ(list2[3], 999);
			EXPECT_EQ(list2[4],   1);
			EXPECT_EQ(list2[5],   2);
			EXPECT_EQ(list2[6],   3);
		}
		EXPECT_TRUE(TDebugItem::Check());
	}

	TEST(io_collection_list, ForEachLoop)
	{
		TList<int> list = {0,1,2,3,4,5,6,7,8,9};

		unsigned c = 0;
		int last = -1;
		for(int& item : list)
		{
			EXPECT_EQ(item, last + 1);
			EXPECT_EQ(&item, &list[c]);
			last = item;
			c++;
		}

		EXPECT_EQ(c, 10U);
	}

	TEST(io_collection_list, Pipe)
	{
		TList<int> list = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

		int sum = 0;
		unsigned count = 0;
		list.Pipe().ForEach([&](int x) { count++; sum += x; });

		EXPECT_EQ(45, sum);
		EXPECT_EQ(10U, count);

		EXPECT_EQ(10U, list.Pipe().Count());

		EXPECT_EQ(0, list.Pipe().First());

		EXPECT_EQ(10,  list.Pipe().Filter([](int x) { return x == 5; }).Map([](int x) { return x+5; }).First());
		EXPECT_EQ(1U, list.Pipe().Filter([](int x) { return x == 5; }).Count());
		EXPECT_EQ(5,  list.Pipe().Filter([](int x) { return x == 5; }).First());

		auto list2 = list.Pipe().Filter([](int x) { return x > 5; }).Collect();
		EXPECT_EQ(4U, list2.Count());
		EXPECT_EQ(6, list2[0]);

		EXPECT_EQ(2U, Concat(list.Pipe(), list.Pipe()).Filter([](int x) { return x == 5; }).Count());
	}
}
