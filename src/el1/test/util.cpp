#include "util.hpp"
#include <math.h>
#include <el1/io_bcd.hpp>
#include <el1/util.hpp>

namespace el1::io::text::string
{
	void PrintTo(const TString& str, std::ostream* os)
	{
		(*os) << str.MakeCStr().get();
	}
}

namespace el1::system::time
{
	std::ostream& operator<<(std::ostream& os, const TTime t)
	{
		auto x = os.precision();
		os.precision(3);
		os<<std::fixed<<t.ConvertToF(EUnit::SECONDS)<<"s {sec: "<<t.Seconds()<<"; asec: "<<t.Attoseconds()<<"}";
		os.precision(x);
		return os;
	}

	void PrintTo(const TTime t, std::ostream* os)
	{
		*os<<t;
	}
}

namespace el1::io::bcd
{
	std::ostream& operator<<(std::ostream& os, const TBCD& v)
	{
		os<<(v.IsNegative() ? "-" : "+");
		for(int i = v.CountInteger() - 1; i >= -v.CountDecimal(); i--)
		{
			if(i == -1)
				os<<".";
			os<<(int)v.Digit(i);
		}
		return os;
	}

	void PrintTo(const TBCD& v, std::ostream* os)
	{
		*os<<v;
	}
}

namespace el1::testing
{
	TString TDebugException::Message() const
	{
		return "debug exception";
	}

	IException* TDebugException::Clone() const
	{
		return new TDebugException(*this);
	}

	unsigned long long TDebugItem::n_constructed = 0;
	unsigned long long TDebugItem::n_destructed = 0;
	unsigned long long TDebugItem::n_compares = 0;
	unsigned long long TDebugItem::n_assigned = 0;
	unsigned long long TDebugItem::n_copy_before_throw = (unsigned long long)-1L;
	unsigned long long TDebugItem::n_move_before_throw = (unsigned long long)-1L;
	unsigned long long TDebugItem::n_assign_before_throw = (unsigned long long)-1L;

	bool operator==(int lhs, const TDebugItem& rhs)
	{
		return rhs.value == lhs;
	}
}

namespace
{
	using namespace el1::util;
	using namespace ::testing;

	usys_t RunBinarySearch(const int* const arr, const usys_t n_items, const int needle)
	{
		return BinarySearch(
			[&](const usys_t idx)
			{
				// std::cerr<<"idx = "<<idx<<"; needle = "<<needle<<std::endl;
				if(arr[idx] > needle)
					return 1;
				else if(arr[idx] < needle)
					return -1;
				else
					return 0;
			},
			n_items
		);
	}

	TEST(util, BinarySearch)
	{
		{
			const int arr[] = { 0,1,2,3,4,5,6,7,8,9 };
			const usys_t n_items = sizeof(arr) / sizeof(arr[0]);
			for(usys_t i = 0; i < n_items; i++)
				EXPECT_EQ(RunBinarySearch(arr, n_items, i), i);

			EXPECT_EQ(RunBinarySearch(arr, n_items,  100), NEG1);
			EXPECT_EQ(RunBinarySearch(arr, n_items, -100), NEG1);
		}

		{
			const int arr[] = { 0,1,2,3,4,5,6,7,8 };
			const usys_t n_items = sizeof(arr) / sizeof(arr[0]);
			for(usys_t i = 0; i < n_items; i++)
				EXPECT_EQ(RunBinarySearch(arr, n_items, i), i);

			EXPECT_EQ(RunBinarySearch(arr, n_items,  100), NEG1);
			EXPECT_EQ(RunBinarySearch(arr, n_items, -100), NEG1);
		}
	}

	TEST(util, NumericHelpers)
	{
		EXPECT_EQ(Abs(-7), 7);
		EXPECT_EQ(Abs(7), 7);
		EXPECT_DOUBLE_EQ(Abs(-2.5), 2.5);

		EXPECT_EQ(Max(3), 3);
		EXPECT_EQ(Max(3, 5), 5);
		EXPECT_EQ(Max(3, 5, 2, 9, 1), 9);
		EXPECT_EQ(Min(3), 3);
		EXPECT_EQ(Min(3, 5), 3);
		EXPECT_EQ(Min(3, 5, 2, 9, 1), 1);

		EXPECT_EQ(ModCeil(0U, 8U), 0U);
		EXPECT_EQ(ModCeil(8U, 8U), 8U);
		EXPECT_EQ(ModCeil(9U, 8U), 16U);
	}

	TEST(util, SwapAndCoalesce)
	{
		int a = 1;
		int b = 2;
		Swap(a, b);
		EXPECT_EQ(a, 2);
		EXPECT_EQ(b, 1);

		int c = 3;
		int* const null = nullptr;
		EXPECT_EQ(Coalesce(null, &a), &a);
		EXPECT_EQ(Coalesce(&b, &a), &b);
		EXPECT_EQ(Coalesce(null, null, &c, &a), &c);
	}

	TEST(util, BitPositionHelpers)
	{
		EXPECT_EQ(CountLeadingZeroes((u32_t)0), 32);
		EXPECT_EQ(CountLeadingZeroes((u32_t)1), 31);
		EXPECT_EQ(CountLeadingZeroes((u32_t)0x80000000U), 0);
		EXPECT_EQ(CountLeadingZeroes((u64_t)0), 64);
		EXPECT_EQ(CountLeadingZeroes((u64_t)1), 63);
		EXPECT_EQ(CountLeadingZeroes((u64_t)0x8000000000000000ULL), 0);

		EXPECT_EQ(FindFirstSet((u32_t)0), 0);
		EXPECT_EQ(FindFirstSet((u32_t)8), 4);
		EXPECT_EQ(FindFirstSet((u64_t)0), 0);
		EXPECT_EQ(FindFirstSet((u64_t)0x100000000ULL), 33);

		EXPECT_EQ(CountTrailingZeroes((u32_t)0), 32);
		EXPECT_EQ(CountTrailingZeroes((u32_t)1), 0);
		EXPECT_EQ(CountTrailingZeroes((u32_t)8), 3);
		EXPECT_EQ(CountTrailingZeroes((u64_t)0), 64);
		EXPECT_EQ(CountTrailingZeroes((u64_t)0x100000000ULL), 32);

		EXPECT_EQ(FindLastSet((u32_t)0), 0);
		EXPECT_EQ(FindLastSet((u32_t)8), 4);
		EXPECT_EQ(FindLastSet((u64_t)0), 0);
		EXPECT_EQ(FindLastSet((u64_t)0x100000000ULL), 33);
	}

	TEST(util, BinarySearchEmptyAndSubrange)
	{
		EXPECT_EQ(BinarySearch([](const usys_t) { return 1; }, 0U), (ssys_t)NEG1);

		const int values[] = { 1, 3, 5, 7, 9 };
		const auto compare = [&](const usys_t index)
		{
			if(values[index] < 7)
				return -1;
			if(values[index] > 7)
				return 1;
			return 0;
		};
		EXPECT_EQ(BinarySearch(compare, 2, 4), 3);
	}

}
