#include "io_bcd.hpp"
#include "util.hpp"
#include <string.h>
#include <endian.h>
#include <math.h>

namespace el1::io::bcd
{
	using namespace error;
	using namespace math;
	using namespace io::collection::list;

	array_t<const digit_t> TBCD::Digits() const noexcept
	{
		return array_t<const digit_t>(DigitsPointer(), DigitsPointer() == nullptr ? 0 : n_decimal + n_integer);
	}

	array_t<const digit_t> TBCD::IntegerDigits() const noexcept
	{
		return array_t<const digit_t>(DigitsPointer() == nullptr ? nullptr : DigitsPointer() + n_decimal, DigitsPointer() == nullptr ? 0 : n_integer);
	}

	array_t<const digit_t> TBCD::DecimalDigits() const noexcept
	{
		return array_t<const digit_t>(DigitsPointer(), DigitsPointer() == nullptr ? 0 : n_decimal);
	}

	/*****************************************************************/
	// MATH OPERATIONS

	int  TBCD::AbsAdd(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(out.base != lhs.base || out.base != rhs.base, TLogicException);
		out.is_zero = 0;

		int carry = 0;
		for(int i = -out.n_decimal; i < out.n_integer; i++)
		{
			int v = (int)lhs.Digit(i) + (int)rhs.Digit(i) + carry;

			carry = 0;
			while(v >= out.base)
			{
				carry++;
				v -= out.base;
			}

			out.Digit(i, (digit_t)v);
		}

		return carry;
	}

	int  TBCD::AbsSub(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(out.base != lhs.base || out.base != rhs.base, TLogicException);
		out.is_zero = 0;

		int carry = 0;
		for(int i = -out.n_decimal; i < out.n_integer; i++)
		{
			int v = (int)lhs.Digit(i) - (int)rhs.Digit(i) - carry;

			carry = 0;
			while(v < 0)
			{
				carry++;
				v += out.base;
			}

			out.Digit(i, (digit_t)v);
		}

		return carry;
	}

	void TBCD::AbsMul(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(out.base != lhs.base || out.base != rhs.base, TLogicException);
		out.is_zero = 0;

		TBCD add(0, out.base, out.n_integer + lhs.n_decimal + rhs.n_decimal, out.n_decimal);
		TBCD tmp(0, out.base, out.n_integer + lhs.n_decimal + rhs.n_decimal, 0);

		const unsigned cr = rhs.n_decimal + rhs.n_integer;
		const unsigned cl = lhs.n_decimal + lhs.n_integer;
		const digit_t* const arr_rhs = rhs.DigitsPointer();
		const digit_t* const arr_lhs = lhs.DigitsPointer();
		for(unsigned r = 0; r < cr; r++)
			for(unsigned l = 0; l < cl; l++)
			{
				const int z = (int)(arr_rhs[r]) * (int)(arr_lhs[l]);
				if(z != 0)
				{
					tmp = z;
					tmp <<= (r + l);
					add += tmp;
				}
			}

		add >>= lhs.n_decimal + rhs.n_decimal;

		bool is_negative = out.is_negative;
		out = std::move(add);
		out.is_negative = is_negative;
	}

	void TBCD::AbsDiv(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(out.base != lhs.base || out.base != rhs.base, TLogicException);
		EL_ERROR(rhs.IsZero(), TInvalidArgumentException, "rhs", "divisor cannot be zero");

		TBCD run(lhs, out.base, out.n_integer + 1, out.n_decimal);
		TBCD div(rhs, out.base, out.n_integer + 1, out.n_decimal);
		run.is_negative = 0;
		div.is_negative = 0;

		// std::cerr<<"begin run = "<<run<<'\n';
		// std::cerr<<"begin div = "<<div<<'\n';

		const bool is_negative = out.is_negative;
		out.SetZero();
		out.EnsureDigits();
		out.is_negative = is_negative;

		int shift = run.IndexMostSignificantNonZeroDigit() - div.IndexMostSignificantNonZeroDigit();
		digit_t f;
		div.Shift(shift);

		while(div < run && !div.IsZero())
		{
			div <<= 1;
			shift++;
		}

		// std::cerr<<"begin shift = "<<shift<<'\n';

		while(!run.IsZero())
		{
			// std::cerr<<"head run = "<<run<<'\n';
			// std::cerr<<"head div = "<<div<<'\n';
			// std::cerr<<"head shift = "<<shift<<'\n';

			while(run < div && !div.IsZero())
			{
				div >>= 1;
				shift--;
			}

			// std::cerr<<"new shift = "<<shift<<'\n';

			if(div.IsZero() || -shift > (int)out.n_decimal)
				break;

			// std::cerr<<"run = "<<run<<'\n';
			// std::cerr<<"div = "<<div<<'\n';

			for(f = 0; run >= div; f++)
				run -= div;

			// std::cerr<<"f   = "<<(int)f<<'\n';
			// std::cerr<<"run = "<<run<<'\n';

			// 0.13 : 0.14  => shift
			// 0.13 : 0.01  => 13 => TLogicException
			// 0.13 : 0.014

			if(f < out.base)
				out.Digit(shift, f);
			else
			{
				TBCD tmp(f, out);
				tmp.Shift(shift);
				out += tmp;
			}
			// std::cerr<<"new out = "<<out<<'\n';
		}

		// std::cerr<<"end out = "<<out<<'\n';
	}

	int TBCD::Add(TBCD& out, const TBCD& _lhs, const TBCD& _rhs)
	{
		TBCD lhs_copy(0, out);
		TBCD rhs_copy(0, out);
		if(out.base != _lhs.base) lhs_copy = _lhs;
		if(out.base != _rhs.base) rhs_copy = _rhs;
		const TBCD& lhs = out.base == _lhs.base ? _lhs : lhs_copy;
		const TBCD& rhs = out.base == _rhs.base ? _rhs : rhs_copy;

		if(lhs.is_negative == rhs.is_negative)
		{
			out.is_negative = lhs.is_negative;
			return AbsAdd(out, lhs, rhs);
		}
		else
		{
			// -7 +  2 => -(7 - 2)
			//  7 + -2 => +(7 - 2)

			//  7 + -8 => -(8 - 7)
			// -7 +  8 => +(8 - 7)

			const int cmp = lhs.Compare(rhs, true);
			if(cmp > 0)
			{
				// no swap
				return AbsSub(out, lhs, rhs);
			}
			else if(cmp < 0)
			{
				// swap
				out.is_negative = rhs.is_negative;
				return AbsSub(out, rhs, lhs);
			}
			else
			{
				out.SetZero();
				return 0;
			}
		}
	}

	int TBCD::Subtract(TBCD& out, const TBCD& _lhs, const TBCD& _rhs)
	{
		TBCD lhs_copy(0, out);
		TBCD rhs_copy(0, out);
		if(out.base != _lhs.base) lhs_copy = _lhs;
		if(out.base != _rhs.base) rhs_copy = _rhs;
		const TBCD& lhs = out.base == _lhs.base ? _lhs : lhs_copy;
		const TBCD& rhs = out.base == _rhs.base ? _rhs : rhs_copy;

		// -7 -  2 => -(7 + 2) [DONE]
		//  7 - -2 => +(7 + 2) [DONE]
		// -7 - -2 => -(7 - 2) [DONE]
		//  7 -  2 => +(7 - 2) [DONE]

		// -5 -  8 => -(5 + 8) [DONE]
		//  5 - -8 => +(5 + 8) [DONE]
		// -5 - -8 => +(8 - 5) [DONE]
		//  5 -  8 => -(8 - 5) [DONE]

		// 122 - 277 => -(277 - 122)

		const int cmp = lhs.Compare(rhs, true);
		if(cmp > 0 || (cmp == 0 && lhs.is_negative != rhs.is_negative))
		{
			// do not assign to out.is_negative here (lhs or rhs might shadow out)
			if(lhs.is_negative != rhs.is_negative)
			{
				// one of the two numbers is negative the other positive
				out.is_negative = !rhs.is_negative;
				return AbsAdd(out, lhs, rhs);
			}
			else
			{
				out.is_negative = rhs.is_negative;
				return AbsSub(out, lhs, rhs);
			}
		}
		else if(cmp < 0)
		{
			if(lhs.is_negative != rhs.is_negative)
			{
				out.is_negative = !rhs.is_negative;
				return AbsAdd(out, lhs, rhs);
			}
			else
			{
				// swap
				out.is_negative = !rhs.is_negative;
				return AbsSub(out, rhs, lhs);
			}
		}
		else
		{
			out.SetZero();
			return 0;
		}
	}

	void TBCD::Multiply(TBCD& out, const TBCD& _lhs, const TBCD& _rhs)
	{
		if(_lhs.IsZero() || _rhs.IsZero())
		{
			out.SetZero();
			return;
		}

		TBCD lhs_copy(0, out);
		TBCD rhs_copy(0, out);
		if(out.base != _lhs.base) lhs_copy = _lhs;
		if(out.base != _rhs.base) rhs_copy = _rhs;
		const TBCD& lhs = out.base == _lhs.base ? _lhs : lhs_copy;
		const TBCD& rhs = out.base == _rhs.base ? _rhs : rhs_copy;

		out.is_negative = lhs.is_negative != rhs.is_negative;
		AbsMul(out, lhs, rhs);
	}

	void TBCD::Divide(TBCD& out, const TBCD& _lhs, const TBCD& _rhs)
	{
		if(_lhs.IsZero())
		{
			out.SetZero();
			return;
		}

		TBCD lhs_copy(0, out);
		TBCD rhs_copy(0, out);
		if(out.base != _lhs.base) lhs_copy = _lhs;
		if(out.base != _rhs.base) rhs_copy = _rhs;
		const TBCD& lhs = out.base == _lhs.base ? _lhs : lhs_copy;
		const TBCD& rhs = out.base == _rhs.base ? _rhs : rhs_copy;

		out.is_negative = lhs.is_negative != rhs.is_negative;
		AbsDiv(out, lhs, rhs);
	}

	TBCD& TBCD::operator+=(const TBCD& rhs)
	{
		Add(*this, *this, rhs);
		return *this;
	}

	TBCD& TBCD::operator-=(const TBCD& rhs)
	{
		Subtract(*this, *this, rhs);
		return *this;
	}

	TBCD& TBCD::operator*=(const TBCD& rhs)
	{
		Multiply(*this, *this, rhs);
		return *this;
	}

	TBCD& TBCD::operator/=(const TBCD& rhs)
	{
		Divide(*this, *this, rhs);
		return *this;
	}

	TBCD& TBCD::operator%=(const TBCD& rhs)
	{
		EL_NOT_IMPLEMENTED;
		// *this = Divide(*this, *this, rhs);
		return *this;
	}

	TBCD& TBCD::operator<<=(const unsigned n_shift)
	{
		if(n_shift == 0)
			return *this;

		if(n_shift >= n_decimal + n_integer)
		{
			SetZero();
			return *this;
		}

		// shift up
		if(DigitsPointer())
		{
			memmove(DigitsPointer() + n_shift, DigitsPointer(), (n_decimal + n_integer - n_shift) * sizeof(digit_t));
			memset(DigitsPointer(), 0, n_shift * sizeof(digit_t));
		}

		return *this;
	}

	TBCD& TBCD::operator>>=(const unsigned n_shift)
	{
		if(n_shift == 0)
			return *this;

		if(n_shift >= n_decimal + n_integer)
		{
			SetZero();
			return *this;
		}

		// shift down
		if(DigitsPointer())
		{
			memmove(DigitsPointer(), DigitsPointer() + n_shift, (n_decimal + n_integer - n_shift) * sizeof(digit_t));
			memset(DigitsPointer() + (n_decimal + n_integer - n_shift) * sizeof(digit_t), 0, n_shift * sizeof(digit_t));
		}

		return *this;
	}

	/*****************************************************************/
	// COMPARISON

	int TBCD::Compare(const TBCD& _rhs, const bool absolute) const
	{
		if(this->IsZero() && _rhs.IsZero())
			return 0;

		if(!absolute)
		{
			if(this->is_negative == 0 && _rhs.is_negative == 1)
				return 1;

			if(this->is_negative == 1 && _rhs.is_negative == 0)
				return -1;
		}

		TBCD rhs_copy(0, *this);
		if(this->base != _rhs.base) rhs_copy = _rhs;
		const TBCD& rhs = this->base == _rhs.base ? _rhs : rhs_copy;

		const auto [idx_low,idx_high] = this->OuterBounds(rhs);
		for(int i = idx_high; i >= idx_low; i--)
		{
			const digit_t l = this->Digit(i);
			const digit_t r = rhs.Digit(i);

			if(l > r)
				return is_negative && !absolute ? -1 : 1;

			if(l < r)
				return is_negative && !absolute ? 1 : -1;
		}

		return 0;
	}

	bool TBCD::HasSameSpecs(const TBCD& rhs) const
	{
		return this->base == rhs.base && this->n_integer == rhs.n_integer && this->n_decimal == rhs.n_decimal;
	}

	/*****************************************************************/
	// UTILITY FUNCTIONS

	u8_t TBCD::RequiredDigits(const digit_t target_base, const digit_t source_base, const unsigned n_source_digits)
	{
		EL_NOT_IMPLEMENTED;
	}

	std::tuple<int,int> TBCD::OuterBounds(const TBCD& rhs) const
	{
		return {
			-util::Max<int>(this->n_decimal, rhs.n_decimal),
			 util::Max<int>(this->n_integer, rhs.n_integer) - 1
		};
	}

	std::tuple<int,int> TBCD::InnerBounds(const TBCD& rhs) const
	{
		return {
			-util::Min<int>(this->n_decimal, rhs.n_decimal),
			 util::Min<int>(this->n_integer, rhs.n_integer) - 1
		};
	}

	void TBCD::Shift(const int s)
	{
		if(s > 0)
			(*this) <<= s;
		else if(s < 0)
			(*this) >>= -s;
	}

	void TBCD::Round(const u8_t n_decimal_max, const ERoundingMode mode)
	{
		if(n_decimal_max >= n_decimal || IsZero())
			return;

		int carry = 0;
		switch(mode)
		{
			case ERoundingMode::TO_NEAREST:
				carry = Digit(-n_decimal_max - 1) >= 5 ? 1 : 0;
				break;
			case ERoundingMode::TOWARDS_ZERO:
				carry = 0;
				break;
			case ERoundingMode::AWAY_FROM_ZERO:
				carry = 1;
				break;
			case ERoundingMode::DOWNWARD:
				carry = is_negative ? 1 : 0;
				break;
			case ERoundingMode::UPWARD:
				carry = is_negative ? 0 : 1;
				break;
		}

		memset(DigitsPointer(), 0, (n_decimal - n_decimal_max) * sizeof(digit_t));

		TBCD bcd_carry(carry, *this);
		bcd_carry >>= n_decimal_max;
		AbsAdd(*this, *this, bcd_carry);
	}

	void TBCD::SetZero() noexcept
	{
		if(DigitsPointer() && is_zero == 0)
			memset(DigitsPointer(), 0, (n_decimal + n_integer) * sizeof(digit_t));

		is_negative = 0;
		is_zero = 1;
	}

	bool TBCD::IsZero() const noexcept
	{
		if(is_zero)
			return true;

		if(DigitsPointer())
		{
			for(unsigned i = 0; i < n_integer + n_decimal; i++)
				if(DigitsPointer()[i] != 0)
					return false;

			is_zero = 1;
		}

		return true;
	}

	double TBCD::ToDouble() const
	{
		if(IsZero())
			return 0;

		double v = 0;

		for(unsigned i = 0; i < n_integer; i++)
		{
			const double d = DigitsPointer()[n_decimal + i];
			const double m = powl((double)base, (double)i);
			v += d * m;
		}

		for(unsigned i = 1; i <= n_decimal; i++)
		{
			const double d = DigitsPointer()[n_decimal - i];
			const double m = pow((double)base, (double)i);
			// fprintf(stderr, "v0 = %0.20f, d = %f, m = %0.20f\n", v, d, m);
			v += d / m;
			// fprintf(stderr, "v1 = %0.20f\n\n", v);
		}

		if(is_negative)
			v *= -1.0;

		return v;
	}

	s64_t TBCD::ToSignedInt() const
	{
		if(IsZero())
			return 0;

		s64_t v = 0;
		s64_t m = 1;
		for(const u64_t d : IntegerDigits())
		{
			v += d * m;
			m *= base;
		}

		if(is_negative)
			v *= (s64_t)-1;

		return v;
	}

	u64_t TBCD::ToUnsignedInt() const
	{
		if(IsZero())
			return 0;

		u64_t v = 0;
		u64_t m = 1;
		for(const u64_t d : IntegerDigits())
		{
			v += d * m;
			m *= base;
		}
		return v;
	}

	TBCD TBCD::ToBCDInt() const
	{
		return TBCD(*this, base, n_integer, 0);
	}

	u8_t TBCD::CountLeadingZeros() const
	{
		if(DigitsPointer())
			for(u8_t i = 0; i < n_integer; i++)
				if(DigitsPointer()[n_decimal + n_integer - i - 1] != 0)
					return i;

		return n_integer;
	}

	u8_t TBCD::CountTrailingZeros() const
	{
		if(DigitsPointer())
			for(u8_t i = 0; i < n_decimal; i++)
				if(DigitsPointer()[i] != 0)
					return i;

		return n_decimal;
	}

	int TBCD::IndexMostSignificantNonZeroDigit() const
	{
		for(int i = n_integer - 1; i >= -n_decimal; i--)
			if(Digit(i) != 0)
				return i;
		EL_THROW(TInvalidArgumentException, "*this", "cannot have a zero value");
	}

	digit_t TBCD::Digit(const int index) const
	{
		if(DigitsPointer() == nullptr || is_zero)
			return 0;

		const int i = index + n_decimal;
		if(i < 0 || i >= n_decimal + n_integer)
			return 0;

		return DigitsPointer()[i];
	}

	bool TBCD::Digit(const int index, const digit_t d)
	{
		const int i = index + n_decimal;
		if(i < 0 || i >= n_decimal + n_integer)
			return false;

		EnsureDigits();

		if(d != 0) is_zero = 0;
		DigitsPointer()[i] = d;
		return true;
	}

	/*****************************************************************/
	// CONSTRUCTORS + ASSIGNMENT

	void TBCD::InitMem()
	{
		EL_ERROR(sizeof(uptr_t) != sizeof(void*), TLogicException);
		if(InternalMemory())
			memset(_mem, 0, sizeof(_mem));
		else
			new (_mem + sizeof(_mem) - sizeof(uptr_t*)) uptr_t(nullptr);
	}

	digit_t* TBCD::DigitsPointer()
	{
		if(InternalMemory())
			return reinterpret_cast<digit_t*>(_mem);
		else
			return reinterpret_cast<uptr_t*>(_mem + sizeof(_mem) - sizeof(uptr_t))->get();
	}

	const digit_t* TBCD::DigitsPointer() const
	{
		return const_cast<TBCD*>(this)->DigitsPointer();
	}

	TBCD::~TBCD()
	{
		if(!InternalMemory())
			reinterpret_cast<uptr_t*>(_mem + sizeof(_mem) - sizeof(uptr_t))->~uptr_t();
	}

	void TBCD::EnsureDigits()
	{
		if(DigitsPointer())
			return;

		if(!InternalMemory())
		{
			*reinterpret_cast<uptr_t*>(_mem + sizeof(_mem) - sizeof(uptr_t)) = uptr_t(new digit_t[n_decimal + n_integer]);
			memset(DigitsPointer(), 0, (n_decimal + n_integer) * sizeof(digit_t));
			is_zero = 1;
		}
	}

	template<typename T>
	void TBCD::ConvertInteger(T value)
	{
		EnsureDigits();
		SetZero();

		is_negative = value < 0;
		is_zero = value == 0;

		if(is_negative)
			value *= (T)-1;

		if(is_zero == 0)
			for(int i = 0; value != 0 && i < n_integer; i++)
			{
				const T d = value % (T)base;
				value /= (T)base;
				DigitsPointer()[n_decimal + i] = (digit_t)d;
			}
	}

	void TBCD::ConvertFloat(double value)
	{
		EnsureDigits();
		SetZero();

		is_negative = value < 0;
		is_zero = value == 0;

		if(is_negative)
			value *= -1.0;

		if(is_zero == 0)
		{
			double w = value;
			for(int i = 0; i < n_integer; i++)
			{
				const digit_t d = (digit_t)floor(fmod(w, (double)base));
				w /= (double)base;
				DigitsPointer()[n_decimal + i] = (digit_t)d;
			}

			w = value;
			for(int i = 0; i < n_decimal; i++)
			{
				w *= (double)base;
				const digit_t d = (digit_t)floor(fmod(w, (double)base));
				DigitsPointer()[n_decimal - 1 - i] = (digit_t)d;
			}
		}
	}

	void TBCD::ConvertBCD(const TBCD& value)
	{
		if(value.IsZero())
		{
			SetZero();
		}
		else
		{
			EnsureDigits();
			if(HasSameSpecs(value))
			{
				memcpy(DigitsPointer(), value.DigitsPointer(), (n_integer + n_decimal) * sizeof(digit_t));
				is_negative = value.is_negative;
				is_zero = 0;
			}
			else
			{
				if(this->base == value.base)
				{
					// we can just copy the DigitsPointer()
					for(int i = -n_decimal; i < n_integer; i++)
						Digit(i, value.Digit(i));
				}
				else
					EL_NOT_IMPLEMENTED;
			}
		}
	}

	TBCD& TBCD::operator=(TBCD&& rhs)
	{
		if(HasSameSpecs(rhs))
		{
			EnsureDigits();
			memmove(DigitsPointer(), rhs.DigitsPointer(), (n_integer + n_decimal) * sizeof(digit_t));
			is_negative = rhs.is_negative;
			is_zero = rhs.is_zero;
		}
		else
		{
			ConvertBCD(rhs);
		}

		return *this;
	}

	TBCD& TBCD::operator=(const TBCD& rhs)
	{
		ConvertBCD(rhs);
		return *this;
	}

	TBCD& TBCD::operator=(const double rhs)
	{
		if(rhs == 0)
			SetZero();
		else
			ConvertFloat(rhs);
		return *this;
	}

	TBCD& TBCD::operator=(const u64_t rhs)
	{
		if(rhs == 0)
			SetZero();
		else
			ConvertInteger(rhs);
		return *this;
	}

	TBCD& TBCD::operator=(const s64_t rhs)
	{
		if(rhs == 0)
			SetZero();
		else
			ConvertInteger(rhs);
		return *this;
	}

	TBCD& TBCD::operator=(const int rhs)
	{
		if(rhs == 0)
			SetZero();
		else
			ConvertInteger(rhs);
		return *this;
	}

	TBCD::TBCD(float v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer), n_decimal(n_decimal), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(double v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer), n_decimal(n_decimal), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(const TBCD& v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, v.base, v.n_integer)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : RequiredDigits(base, v.base, v.n_decimal)), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(u8_t v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(v) * 8)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : 0), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(s8_t v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(v) * 8)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : 0), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(u16_t v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(v) * 8)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : 0), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(s16_t v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(v) * 8)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : 0), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(u32_t v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(v) * 8)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : 0), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = (u64_t)v;
	}

	TBCD::TBCD(s32_t v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(v) * 8)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : 0), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(u64_t v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(v) * 8)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : 0), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(s64_t v, const digit_t base, const u8_t n_integer, const u8_t n_decimal) : base(base), n_integer(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(v) * 8)), n_decimal(n_decimal != AUTO_DETECT ? n_decimal : 0), is_negative(0), is_zero(1)
	{
		InitMem();
		*this = v;
	}

	TBCD::TBCD(TBCD v, const TBCD& conf_ref) : TBCD(std::move(v), conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const u8_t   v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const s8_t   v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const u16_t  v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const s16_t  v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const u32_t  v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const s32_t  v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const u64_t  v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const s64_t  v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const float  v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const double v, const TBCD& conf_ref) : TBCD(v, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}

	// copy constructor
	TBCD::TBCD(const TBCD& v) : base(v.base), n_integer(v.n_integer), n_decimal(v.n_decimal), is_negative(0), is_zero(1)
	{
		InitMem();
		if(!v.IsZero())
		{
			EnsureDigits();
			memcpy(this->DigitsPointer(), v.DigitsPointer(), (v.n_integer + v.n_decimal) * sizeof(digit_t));
			is_negative = v.is_negative;
			is_zero = 0;
		}
	}

	/*****************************************************************/
	// WRAPPER FUNCTIONS

	TBCD& TBCD::operator+=(const double rhs)
	{
		if(rhs != 0)
			(*this) += TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator-=(const double rhs)
	{
		if(rhs != 0)
			(*this) -= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator*=(const double rhs)
	{
		if(rhs == 0)
		{
			SetZero();
			return *this;
		}

		if(rhs == 1)
			return *this;

		if(rhs == -1)
		{
			is_negative = 1 - is_negative;
			return *this;
		}

		(*this) *= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator/=(const double rhs)
	{
		if(rhs == 1)
			return *this;

		if(rhs == -1)
		{
			is_negative = 1 - is_negative;
			return *this;
		}

		(*this) /= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator%=(const double rhs)
	{
		(*this) %= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator+=(const u64_t rhs)
	{
		if(rhs != 0)
			(*this) += TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator-=(const u64_t rhs)
	{
		if(rhs != 0)
			(*this) -= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator*=(const u64_t rhs)
	{
		if(rhs == 0)
		{
			SetZero();
			return *this;
		}

		if(rhs == 1)
			return *this;

		(*this) *= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator/=(const u64_t rhs)
	{
		if(rhs == 1)
			return *this;

		(*this) /= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator%=(const u64_t rhs)
	{
		(*this) %= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator+=(const s64_t rhs)
	{
		if(rhs != 0)
			(*this) += TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator-=(const s64_t rhs)
	{
		if(rhs != 0)
			(*this) -= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator*=(const s64_t rhs)
	{
		if(rhs == 0)
		{
			SetZero();
			return *this;
		}

		if(rhs == 1)
			return *this;

		if(rhs == -1)
		{
			is_negative = 1 - is_negative;
			return *this;
		}

		(*this) *= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator/=(const s64_t rhs)
	{
		if(rhs == 1)
			return *this;

		if(rhs == -1)
		{
			is_negative = 1 - is_negative;
			return *this;
		}

		(*this) /= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator%=(const s64_t rhs)
	{
		(*this) %= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator+=(const int rhs)
	{
		if(rhs != 0)
			(*this) += TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator-=(const int rhs)
	{
		if(rhs != 0)
			(*this) -= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator*=(const int rhs)
	{
		if(rhs == 0)
		{
			SetZero();
			return *this;
		}

		if(rhs == 1)
			return *this;

		if(rhs == -1)
		{
			is_negative = 1 - is_negative;
			return *this;
		}

		(*this) *= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator/=(const int rhs)
	{
		if(rhs == 1)
			return *this;

		if(rhs == -1)
		{
			is_negative = 1 - is_negative;
			return *this;
		}

		(*this) /= TBCD(rhs, *this);
		return *this;
	}

	TBCD& TBCD::operator%=(const int rhs)
	{
		(*this) %= TBCD(rhs, *this);
		return *this;
	}

	TBCD TBCD::operator+(const TBCD& rhs) const
	{
		TBCD lhs(*this);
		lhs += rhs;
		return lhs;
	}

	TBCD TBCD::operator-(const TBCD& rhs) const
	{
		TBCD lhs(*this);
		lhs -= rhs;
		return lhs;
	}

	TBCD TBCD::operator*(const TBCD& rhs) const
	{
		TBCD lhs(*this);
		lhs *= rhs;
		return lhs;
	}

	TBCD TBCD::operator/(const TBCD& rhs) const
	{
		TBCD lhs(*this);
		lhs /= rhs;
		return lhs;
	}

	TBCD TBCD::operator%(const TBCD& rhs) const
	{
		TBCD lhs(*this);
		lhs %= rhs;
		return lhs;
	}

	TBCD TBCD::operator+(const double rhs) const
	{
		return (*this) + TBCD(rhs, *this);
	}

	TBCD TBCD::operator-(const double rhs) const
	{
		return (*this) - TBCD(rhs, *this);
	}

	TBCD TBCD::operator*(const double rhs) const
	{
		return (*this) * TBCD(rhs, *this);
	}

	TBCD TBCD::operator/(const double rhs) const
	{
		return (*this) / TBCD(rhs, *this);
	}

	TBCD TBCD::operator%(const double rhs) const
	{
		return (*this) % TBCD(rhs, *this);
	}

	TBCD TBCD::operator+(const u64_t rhs) const
	{
		return (*this) + TBCD(rhs, *this);
	}

	TBCD TBCD::operator-(const u64_t rhs) const
	{
		return (*this) - TBCD(rhs, *this);
	}

	TBCD TBCD::operator*(const u64_t rhs) const
	{
		return (*this) * TBCD(rhs, *this);
	}

	TBCD TBCD::operator/(const u64_t rhs) const
	{
		return (*this) / TBCD(rhs, *this);
	}

	TBCD TBCD::operator%(const u64_t rhs) const
	{
		return (*this) % TBCD(rhs, *this);
	}

	TBCD TBCD::operator+(const s64_t rhs) const
	{
		return (*this) + TBCD(rhs, *this);
	}

	TBCD TBCD::operator-(const s64_t rhs) const
	{
		return (*this) - TBCD(rhs, *this);
	}

	TBCD TBCD::operator*(const s64_t rhs) const
	{
		return (*this) * TBCD(rhs, *this);
	}

	TBCD TBCD::operator/(const s64_t rhs) const
	{
		return (*this) / TBCD(rhs, *this);
	}

	TBCD TBCD::operator%(const s64_t rhs) const
	{
		return (*this) % TBCD(rhs, *this);
	}

	TBCD TBCD::operator+(const int rhs) const
	{
		return (*this) + TBCD(rhs, *this);
	}

	TBCD TBCD::operator-(const int rhs) const
	{
		return (*this) - TBCD(rhs, *this);
	}

	TBCD TBCD::operator*(const int rhs) const
	{
		return (*this) * TBCD(rhs, *this);
	}

	TBCD TBCD::operator/(const int rhs) const
	{
		return (*this) / TBCD(rhs, *this);
	}

	TBCD TBCD::operator%(const int rhs) const
	{
		return (*this) % TBCD(rhs, *this);
	}

	TBCD TBCD::operator<<(const unsigned n_shift) const
	{
		TBCD lhs = *this;
		lhs <<= n_shift;
		return lhs;
	}

	TBCD TBCD::operator>>(const unsigned n_shift) const
	{
		TBCD lhs = *this;
		lhs >>= n_shift;
		return lhs;
	}

	bool TBCD::operator==(const TBCD& rhs) const
	{
		return this->Compare(rhs) == 0;
	}

	bool TBCD::operator!=(const TBCD& rhs) const
	{
		return this->Compare(rhs) != 0;
	}

	bool TBCD::operator>=(const TBCD& rhs) const
	{
		return this->Compare(rhs) >= 0;
	}

	bool TBCD::operator<=(const TBCD& rhs) const
	{
		return this->Compare(rhs) <= 0;
	}

	bool TBCD::operator> (const TBCD& rhs) const
	{
		return this->Compare(rhs) > 0;
	}

	bool TBCD::operator< (const TBCD& rhs) const
	{
		return this->Compare(rhs) < 0;
	}

	bool TBCD::operator==(const double rhs) const
	{
		if(this->IsZero() && rhs == 0)
			return true;
		if(this->IsZero() && rhs != 0)
			return false;
		return (*this) == TBCD(rhs, *this);
	}

	bool TBCD::operator!=(const double rhs) const
	{
		if(this->IsZero() && rhs != 0)
			return true;
		if(this->IsZero() && rhs == 0)
			return false;
		return (*this) != TBCD(rhs, *this);
	}

	bool TBCD::operator>=(const double rhs) const
	{
		if(this->IsZero() && rhs == 0)
			return true;
		if(this->IsZero() && rhs < 0)
			return true;
		if(this->IsZero() && rhs > 0)
			return false;
		return (*this) >= TBCD(rhs, *this);
	}

	bool TBCD::operator<=(const double rhs) const
	{
		if(rhs == 0 && this->IsZero())
			return true;
		if(rhs > 0 && this->IsZero())
			return true;
		if(rhs < 0 && this->IsZero())
			return false;
		return (*this) <= TBCD(rhs, *this);
	}

	bool TBCD::operator> (const double rhs) const
	{
		if(rhs < 0 && this->IsZero())
			return true;
		if(rhs > 0 && this->IsZero())
			return false;
		return (*this) > TBCD(rhs, *this);
	}

	bool TBCD::operator< (const double rhs) const
	{
		if(rhs > 0 && this->IsZero())
			return true;
		if(rhs < 0 && this->IsZero())
			return false;
		return (*this) < TBCD(rhs, *this);
	}

	bool TBCD::operator==(const u64_t rhs) const
	{
		if(rhs == 0 && this->IsZero())
			return true;
		if(rhs != 0 && this->IsZero())
			return false;
		return (*this) == TBCD(rhs, *this);
	}

	bool TBCD::operator!=(const u64_t rhs) const
	{
		if(rhs != 0 && this->IsZero())
			return true;
		if(rhs == 0 && this->IsZero())
			return false;
		return (*this) != TBCD(rhs, *this);
	}

	bool TBCD::operator>=(const u64_t rhs) const
	{
		if(this->IsZero() && rhs == 0)
			return true;
		if(this->IsZero() && rhs > 0)
			return false;
		return (*this) >= TBCD(rhs, *this);
	}

	bool TBCD::operator<=(const u64_t rhs) const
	{
		if(rhs == 0 && this->IsZero())
			return true;
		if(rhs > 0 && this->IsZero())
			return true;
		return (*this) <= TBCD(rhs, *this);
	}

	bool TBCD::operator> (const u64_t rhs) const
	{
		if(rhs > 0 && this->IsZero())
			return false;
		return (*this) > TBCD(rhs, *this);
	}

	bool TBCD::operator< (const u64_t rhs) const
	{
		if(rhs > 0 && this->IsZero())
			return true;
		return (*this) < TBCD(rhs, *this);
	}

	bool TBCD::operator==(const s64_t rhs) const
	{
		if(this->IsZero() && rhs == 0)
			return true;
		if(this->IsZero() && rhs != 0)
			return false;
		return (*this) == TBCD(rhs, *this);
	}

	bool TBCD::operator!=(const s64_t rhs) const
	{
		if(rhs != 0 && this->IsZero())
			return true;
		if(rhs == 0 && this->IsZero())
			return false;
		return (*this) != TBCD(rhs, *this);
	}

	bool TBCD::operator>=(const s64_t rhs) const
	{
		if(this->IsZero() && rhs == 0)
			return true;
		if(this->IsZero() && rhs < 0)
			return true;
		if(this->IsZero() && rhs > 0)
			return false;
		return (*this) >= TBCD(rhs, *this);
	}

	bool TBCD::operator<=(const s64_t rhs) const
	{
		if(rhs == 0 && this->IsZero())
			return true;
		if(rhs > 0 && this->IsZero())
			return true;
		if(rhs < 0 && this->IsZero())
			return false;
		return (*this) <= TBCD(rhs, *this);
	}

	bool TBCD::operator> (const s64_t rhs) const
	{
		if(rhs < 0 && this->IsZero())
			return true;
		if(rhs > 0 && this->IsZero())
			return false;
		return (*this) > TBCD(rhs, *this);
	}

	bool TBCD::operator< (const s64_t rhs) const
	{
		if(rhs > 0 && this->IsZero())
			return true;
		if(rhs < 0 && this->IsZero())
			return false;
		return (*this) < TBCD(rhs, *this);
	}

	bool TBCD::operator==(const int rhs) const
	{
		if(this->IsZero() && rhs == 0)
			return true;
		if(this->IsZero() && rhs != 0)
			return false;
		return (*this) == TBCD(rhs, *this);
	}

	bool TBCD::operator!=(const int rhs) const
	{
		if(rhs != 0 && this->IsZero())
			return true;
		if(rhs == 0 && this->IsZero())
			return false;
		return (*this) != TBCD(rhs, *this);
	}

	bool TBCD::operator>=(const int rhs) const
	{
		if(this->IsZero() && rhs == 0)
			return true;
		if(this->IsZero() && rhs < 0)
			return true;
		if(this->IsZero() && rhs > 0)
			return false;
		return (*this) >= TBCD(rhs, *this);
	}

	bool TBCD::operator<=(const int rhs) const
	{
		if(rhs == 0 && this->IsZero())
			return true;
		if(rhs > 0 && this->IsZero())
			return true;
		if(rhs < 0 && this->IsZero())
			return false;
		return (*this) <= TBCD(rhs, *this);
	}

	bool TBCD::operator> (const int rhs) const
	{
		if(rhs < 0 && this->IsZero())
			return true;
		if(rhs > 0 && this->IsZero())
			return false;
		return (*this) > TBCD(rhs, *this);
	}

	bool TBCD::operator< (const int rhs) const
	{
		if(rhs > 0 && this->IsZero())
			return true;
		if(rhs < 0 && this->IsZero())
			return false;
		return (*this) < TBCD(rhs, *this);
	}

	TBCD::operator double() const
	{
		return this->ToDouble();
	}

	TBCD::operator s64_t() const
	{
		return this->ToSignedInt();
	}

	TBCD::operator u64_t() const
	{
		return this->ToUnsignedInt();
	}
}
