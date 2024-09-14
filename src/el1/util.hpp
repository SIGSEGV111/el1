#pragma once

#ifdef _MSC_VER
	#include <intrin.h>
#endif

#include "io_types.hpp"

#include <iostream>

namespace el1::util
{
	using namespace io::types;

	template<typename T>
	static T Abs(const T v)
	{
		return v < 0 ? -v : v;
	}

	template<typename T>
	constexpr T Max(const T a)
	{
		return a;
	}

	template<typename T>
	constexpr T Max(const T a, const T b)
	{
		return a > b ? a : b;
	}

	template<typename T, typename ... A>
	constexpr T Max(const T a, const T b, const A ... other)
	{
		return Max(Max(a, b), Max(static_cast<T>(other)...));
	}

	template<typename T>
	constexpr T Min(const T a)
	{
		return a;
	}

	template<typename T>
	constexpr T Min(const T a, const T b)
	{
		return a < b ? a : b;
	}

	template<typename T, typename ... A>
	constexpr T Min(const T a, const T b, const A ... other)
	{
		return Min(Min(a, b), Min(static_cast<T>(other)...));
	}

	template<typename T>
	void Swap(T& a, T& b)
	{
		T tmp = std::move(a);
		a = std::move(b);
		b = std::move(tmp);
	}

	// returns smalles multiple of "div" that is not less than value
	template<typename T>
	T ModCeil(const T value, const T div)
	{
		const T mod = value % div;
		return (mod == 0) ? value : value + (div - mod);
	}

	#ifdef __GNUG__
		//	Returns the number of leading 0-bits in x, starting at the most significant bit position.
		inline static int CountLeadingZeroes(u32_t v) { return v == 0 ? (32) : (sizeof(int ) == 4 ? __builtin_clz (v) : __builtin_clzl (v)); }
		inline static int CountLeadingZeroes(u64_t v) { return v == 0 ? (64) : (sizeof(long) == 8 ? __builtin_clzl(v) : __builtin_clzll(v)); }

		//	Returns one plus the index of the least significant 1-bit of v, or if v is zero, returns zero.
		inline static int FindFirstSet(u32_t v) { return sizeof(int ) == 4 ? __builtin_ffs (v) : __builtin_ffsl (v); }
		inline static int FindFirstSet(u64_t v) { return sizeof(long) == 8 ? __builtin_ffsl(v) : __builtin_ffsll(v); }

		//	Returns the number of trailing 0-bits in x, starting at the least significant bit position.
		inline static int CountTrailingZeroes(u32_t v) { return v == 0 ? 32 : FindFirstSet(v); }
		inline static int CountTrailingZeroes(u64_t v) { return v == 0 ? 64 : FindFirstSet(v); }

		//	Returns one plus the index of the most significant 1-bit of v, or if v is zero, returns zero.
		inline static int FindLastSet(u32_t v) { return 32 - CountLeadingZeroes(v); }
		inline static int FindLastSet(u64_t v) { return 64 - CountLeadingZeroes(v); }
	#endif

	#ifdef _MSC_VER
		//	Returns the number of leading 0-bits in x, starting at the most significant bit position.
		inline static DWORD CountLeadingZeroes(unsigned __int32 v) { DWORD r; _BitScanReverse(&r, v); return 31-r; }
		inline static DWORD CountLeadingZeroes(unsigned __int64 v);

		//	Returns one plus the index of the least significant 1-bit of v, or if v is zero, returns zero.
		inline static DWORD FindFirstSet(unsigned __int32 v) { DWORD r; return _BitScanReverse(&r, v) ? r : 0; }
		inline static DWORD FindFirstSet(unsigned __int64 v);

		//	Returns the number of trailing 0-bits in x, starting at the least significant bit position.
		inline static int CountTrailingZeroes(unsigned __int32 v) { return v == 0 ? 32 : FindFirstSet(v); }
		inline static int CountTrailingZeroes(unsigned __int64 v) { return v == 0 ? 64 : FindFirstSet(v); }

		//	Returns one plus the index of the most significant 1-bit of v, or if v is zero, returns zero.
		inline static int FindLastSet(unsigned __int32 v) { return 32 - CountLeadingZeroes(v); }
		inline static int FindLastSet(unsigned __int64 v) { return 64 - CountLeadingZeroes(v); }
	#endif

	template<typename L>
	ssys_t BinarySearch(L lambda, ssys_t idx_start, ssys_t idx_end)
	{
		while(idx_start <= idx_end)
		{
			const usys_t idx_mid = (idx_end - idx_start) / 2 + idx_start;
			// std::cerr<<"idx_start = "<<idx_start<<"; idx_end = "<<idx_end<<"; idx_mid = "<<idx_mid<<std::endl;
			const int c = lambda(idx_mid);
			if(c > 0)
			{
				// std::cerr<<"c > 0: search lower\n";
				// search lower half
				idx_end = idx_mid - 1;
			}
			else if(c < 0)
			{
				// std::cerr<<"c < 0: search upper\n";
				// search upper half
				idx_start = idx_mid + 1;
			}
			else
			{
				// std::cerr<<"c == 0: match\n";
				// match
				return idx_mid;
			}
		}

		// std::cerr<<"no match\n";
		// no match
		return NEG1;
	}

	template<typename L>
	ssys_t BinarySearch(L lambda, const usys_t n_items)
	{
		return BinarySearch(lambda, 0, n_items - 1);
	}

	template<typename T>
	T* Coalesce(T* const a, T* const b) {
		return a ? a : b;
	}

	template<typename T, typename... Ts>
	T* Coalesce(T* first, Ts*... rest) {
		return first ? first : Coalesce(rest...);
	}
}
