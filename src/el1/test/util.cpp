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
}
