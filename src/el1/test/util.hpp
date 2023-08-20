#pragma once

#include <gtest/gtest.h>
#include <stdio.h>
#include <el1/io_text_string.hpp>

namespace el1::io::text::string
{
	void PrintTo(const TString& str, std::ostream* os);
}

namespace el1::system::time
{
	std::ostream& operator<<(std::ostream& os, const TTime t);
	void PrintTo(const TTime t, std::ostream* os);
}

namespace el1::testing
{
	using namespace io::text::string;
	using namespace el1::error;

	struct TDebugException : IException
	{
		int number;

		TString Message() const final override;
		IException* Clone() const final override;

		TDebugException(int number) : number(number) {}
	};

	struct TDebugItem
	{
		static unsigned long long n_constructed;
		static unsigned long long n_destructed;
		static unsigned long long n_compares;
		static unsigned long long n_assigned;

		static unsigned long long n_copy_before_throw;
		static unsigned long long n_move_before_throw;
		static unsigned long long n_assign_before_throw;

		static void Reset()
		{
			n_constructed = 0;
			n_destructed = 0;
			n_compares = 0;
			n_copy_before_throw = (unsigned long long)-1L;
			n_move_before_throw = (unsigned long long)-1L;
			n_assign_before_throw = (unsigned long long)-1L;
		}

		static bool Check()
		{
			if(n_constructed != n_destructed)
			{
				fprintf(stderr, "WARNING: n_constructed = %lld, n_destructed = %lld\n", n_constructed, n_destructed);
				return false;
			}

			return true;
		}

		std::unique_ptr<bool> alive;
		int value;

		bool operator==(const TDebugItem& rhs) const
		{
			if(!*alive) throw "compare OF a dead item to another";
			if(!*rhs.alive) throw "compare TO a dead item";

			n_compares++;

			return value == rhs.value;
		}

		TDebugItem& operator=(const TDebugItem& rhs)
		{
			if(!*alive) throw "copy-assigned TO a dead item";
			if(!*rhs.alive) throw "copy-assigned FROM a dead item";
			if(--n_assign_before_throw == 0) throw this;

			n_assigned++;
			value = rhs.value;

			return *this;
		}

		TDebugItem(TDebugItem&& rhs) : alive(std::unique_ptr<bool>(new bool)), value(rhs.value)
		{
			*alive = true;
			rhs.value = -1234567;
			if(!*rhs.alive) throw "copy-constructed from a dead item";
			if(--n_move_before_throw == 0) throw this;
			n_constructed++;
		}

		TDebugItem(const TDebugItem& rhs) : alive(std::unique_ptr<bool>(new bool)), value(rhs.value)
		{
			*alive = true;
			if(!*rhs.alive) throw "copy-constructed from a dead item";
			if(--n_copy_before_throw == 0) throw this;
			n_constructed++;
		}

		TDebugItem(int value) : alive(std::unique_ptr<bool>(new bool)), value(value)
		{
			*alive = true;
			n_constructed++;
		}

		TDebugItem(unsigned value) : alive(std::unique_ptr<bool>(new bool)), value(value)
		{
			*alive = true;
			n_constructed++;
		}

		~TDebugItem()
		{
			if(!alive) fprintf(stderr, "destructed a already dead item");
			*alive = false;
			n_destructed++;
		}
	};

	bool operator==(int lhs, const TDebugItem& rhs);
}
