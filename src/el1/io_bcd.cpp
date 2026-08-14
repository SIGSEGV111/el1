#include "io_bcd.hpp"
#include "io_collection_list.hpp"
#include "io_text_string.hpp"
#include "system_random.hpp"
#include "util.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace el1::io::bcd
{
	using namespace error;
	using namespace math;
	using namespace io::collection::array;
	using namespace io::collection::list;

	TList<digit_t> TBCD::BuildMagnitudeDigits(const TBCD& value, const unsigned target_radix)
	{
		TList<digit_t> result;
		if(value.IsZero())
			return result;

		const auto source = value.Digits();
		result = TList<digit_t>(source);
		detail::TrimDigits(result);
		if(value.Radix() == target_radix)
			return result;
		TList<digit_t> converted;
		detail::ConvertDigitsRadix(converted, result, value.Radix(), target_radix);
		return converted;
	}

	bool TBCD::AssignMagnitudeDigits(TBCD& out, const TList<digit_t>& magnitude, const bool is_negative)
	{
		out.SetZero();
		const usys_t n_digits = out.n_integer + out.n_decimal;
		const usys_t n_copy = util::Min<usys_t>(magnitude.Count(), n_digits);
		if(n_copy != 0)
		{
			out.EnsureDigits();
			memcpy(out.DigitsPointer(), &magnitude[0], n_copy * sizeof(digit_t));
			if(n_copy < n_digits)
				memset(out.DigitsPointer() + n_copy, 0, (n_digits - n_copy) * sizeof(digit_t));
			out.is_zero = 0;
			(void)out.IsZero();
		}
		out.is_negative = is_negative && !out.IsZero();
		return magnitude.Count() > n_digits;
	}

	int TBCD::CompareMagnitude(const TBCD& lhs, const TBCD& rhs)
	{
		if(lhs.Base() == rhs.Base())
		{
			const ssys_t low = -(ssys_t)util::Max(lhs.CountDecimal(), rhs.CountDecimal());
			const ssys_t high = (ssys_t)util::Max(lhs.CountInteger(), rhs.CountInteger()) - 1;
			for(ssys_t i = high; i >= low; i--)
			{
				const digit_t l = lhs.Digit(i);
				const digit_t r = rhs.Digit(i);
				if(l != r)
					return l < r ? -1 : 1;
			}
			return 0;
		}

		// Base 256 minimizes scratch size. The denominators remain implicit and are
		// applied directly to the opposite numerator, so no base^n value is built.
		TList<digit_t> left = BuildMagnitudeDigits(lhs, 256U);
		TList<digit_t> right = BuildMagnitudeDigits(rhs, 256U);
		detail::MultiplyDigitsPower(left, 256U, rhs.Radix(), rhs.CountDecimal());
		detail::MultiplyDigitsPower(right, 256U, lhs.Radix(), lhs.CountDecimal());
		return detail::CompareDigits(left, right);
	}

	int TBCD::AddGeneric(TBCD& out, const TBCD& lhs, const TBCD& rhs, const bool subtract_rhs)
	{
		const unsigned radix = out.Radix();
		TList<digit_t> left = BuildMagnitudeDigits(lhs, radix);
		TList<digit_t> right = BuildMagnitudeDigits(rhs, radix);

		// Put both finite fractions over the same implicit denominator. Only the
		// numerators are materialized; powers are repeated small multiplications.
		detail::MultiplyDigitsPower(left, radix, rhs.Radix(), rhs.CountDecimal());
		detail::MultiplyDigitsPower(right, radix, lhs.Radix(), lhs.CountDecimal());

		const bool left_negative = lhs.IsNegative() && !detail::IsDigitsZero(left);
		const bool right_negative = (rhs.IsNegative() != subtract_rhs) && !detail::IsDigitsZero(right);
		bool result_negative = false;
		TList<digit_t> result;

		if(left_negative == right_negative)
		{
			result = std::move(left);
			detail::AddDigits(result, right, radix);
			result_negative = left_negative;
		}
		else
		{
			const int cmp = detail::CompareDigits(left, right);
			if(cmp == 0)
			{
				out.SetZero();
				return 0;
			}

			if(cmp > 0)
			{
				result = std::move(left);
				detail::SubtractDigits(result, right, radix);
				result_negative = left_negative;
			}
			else
			{
				result = std::move(right);
				detail::SubtractDigits(result, left, radix);
				result_negative = right_negative;
			}
		}

		detail::ShiftDigitsLeft(result, out.CountDecimal());
		detail::DivideDigitsPower(result, radix, lhs.Radix(), lhs.CountDecimal());
		detail::DivideDigitsPower(result, radix, rhs.Radix(), rhs.CountDecimal());
		return AssignMagnitudeDigits(out, result, result_negative) ? 1 : 0;
	}

	void TBCD::MultiplyGeneric(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		const unsigned radix = out.Radix();
		const TList<digit_t> left = BuildMagnitudeDigits(lhs, radix);
		const TList<digit_t> right = BuildMagnitudeDigits(rhs, radix);
		TList<digit_t> result;
		detail::MultiplyDigits(result, left, right, radix);
		const bool negative = (lhs.IsNegative() != rhs.IsNegative()) && !detail::IsDigitsZero(result);

		detail::ShiftDigitsLeft(result, out.CountDecimal());
		detail::DivideDigitsPower(result, radix, lhs.Radix(), lhs.CountDecimal());
		detail::DivideDigitsPower(result, radix, rhs.Radix(), rhs.CountDecimal());
		AssignMagnitudeDigits(out, result, negative);
	}

	TBCD TBCD::DivideGeneric(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		const unsigned out_radix = out.Radix();
		const bool same_radix = out_radix == lhs.Radix() && out_radix == rhs.Radix();
		const unsigned work_radix = same_radix ? out_radix : 256U;
		TList<digit_t> numerator = BuildMagnitudeDigits(lhs, work_radix);
		TList<digit_t> denominator = BuildMagnitudeDigits(rhs, work_radix);
		EL_ERROR(detail::IsDigitsZero(denominator), TInvalidArgumentException, "rhs", "divisor cannot be zero");

		const bool quotient_negative = (lhs.IsNegative() != rhs.IsNegative()) && !detail::IsDigitsZero(numerator);
		const bool remainder_negative = lhs.IsNegative() && !detail::IsDigitsZero(numerator);

		detail::MultiplyDigitsPower(numerator, work_radix, rhs.Radix(), rhs.CountDecimal());
		detail::MultiplyDigitsPower(numerator, work_radix, out_radix, out.CountDecimal());
		detail::MultiplyDigitsPower(denominator, work_radix, lhs.Radix(), lhs.CountDecimal());

		TList<digit_t> quotient;
		TList<digit_t> remainder_digits;
		detail::DivideDigits(quotient, remainder_digits, numerator, denominator, work_radix);
		const bool periodic = !detail::IsDigitsZero(remainder_digits) && detail::IsPeriodicFraction(numerator, denominator, work_radix, out_radix);

		// N = Q*D + R and the common quotient scaling contributes out.Radix()^n_decimal.
		// Removing only the two input denominators therefore yields the returned
		// residual already scaled for `out`.
		detail::DivideDigitsPower(remainder_digits, work_radix, lhs.Radix(), lhs.CountDecimal());
		detail::DivideDigitsPower(remainder_digits, work_radix, rhs.Radix(), rhs.CountDecimal());

		if(work_radix != out_radix)
		{
			TList<digit_t> converted;
			detail::ConvertDigitsRadix(converted, quotient, work_radix, out_radix);
			quotient = std::move(converted);
			TList<digit_t> converted_remainder;
			detail::ConvertDigitsRadix(converted_remainder, remainder_digits, work_radix, out_radix);
			remainder_digits = std::move(converted_remainder);
		}

		TBCD remainder(0, out);
		AssignMagnitudeDigits(remainder, remainder_digits, remainder_negative);
		AssignMagnitudeDigits(out, quotient, quotient_negative);
		out.is_periodic = periodic;
		return remainder;
	}

	TList<digit_t> TBCD::BuildIntegerDigits(u64_t value, const unsigned radix)
	{
		TList<digit_t> result;
		while(value != 0)
		{
			result.Append((digit_t)(value % radix));
			value /= radix;
		}
		return result;
	}

	int TBCD::CompareFloatingParts(const detail::TBinaryFloatParts& rhs) const
	{
		if(IsNaN() || rhs.value_class == EValueClass::NOT_A_NUMBER)
			return 2;

		if(IsInfinity() || rhs.value_class == EValueClass::INFINITE)
		{
			if(IsInfinity() && rhs.value_class == EValueClass::INFINITE)
			{
				if(IsNegative() == rhs.negative)
					return 0;
				return IsNegative() ? -1 : 1;
			}
			if(IsInfinity())
				return IsNegative() ? -1 : 1;
			return rhs.negative ? 1 : -1;
		}

		const bool lhs_zero = IsZero();
		const bool rhs_zero = rhs.significand == 0;
		if(lhs_zero && rhs_zero)
			return 0;

		const bool lhs_negative = IsNegative() && !lhs_zero;
		const bool rhs_negative = rhs.negative && !rhs_zero;
		if(lhs_negative != rhs_negative)
			return lhs_negative ? -1 : 1;

		TList<digit_t> lhs_digits = BuildMagnitudeDigits(*this, 256U);
		TList<digit_t> rhs_digits = BuildIntegerDigits(rhs.significand, 256U);
		if(rhs.exponent >= 0)
		{
			detail::MultiplyDigitsPower(rhs_digits, 256U, 2U, (usys_t)rhs.exponent);
			detail::MultiplyDigitsPower(rhs_digits, 256U, Radix(), CountDecimal());
		}
		else
		{
			detail::MultiplyDigitsPower(lhs_digits, 256U, 2U, (usys_t)-rhs.exponent);
			detail::MultiplyDigitsPower(rhs_digits, 256U, Radix(), CountDecimal());
		}

		int cmp = detail::CompareDigits(lhs_digits, rhs_digits);
		if(lhs_negative)
			cmp = -cmp;
		return cmp;
	}

	int TBCD::CompareFloating(const double rhs) const
	{
		return CompareFloatingParts(detail::DecodeBinaryFloat(rhs));
	}

	namespace
	{
		digit_t RandomDigit(const unsigned radix)
		{
			auto& rng = system::random::TSystemRandom::Instance();
			if(radix == 256U)
				return rng.Integer<digit_t>();

			const unsigned limit = 256U - (256U % radix);
			unsigned value;
			do
			{
				value = rng.Integer<digit_t>();
			}
			while(value >= limit);
			return (digit_t)(value % radix);
		}
	}

	array_t<const digit_t> TBCD::Digits() const noexcept
	{
		if(!IsFinite())
			return {};
		const digit_t* const digits = DigitsPointer();
		return array_t<const digit_t>::FromUnsafePointer(digits, digits == nullptr ? 0 : n_decimal + n_integer);
	}

	array_t<const digit_t> TBCD::IntegerDigits() const noexcept
	{
		if(!IsFinite())
			return {};
		const digit_t* const digits = DigitsPointer();
		return array_t<const digit_t>::FromUnsafePointer(digits == nullptr ? nullptr : digits + n_decimal, digits == nullptr ? 0 : n_integer);
	}

	array_t<const digit_t> TBCD::DecimalDigits() const noexcept
	{
		if(!IsFinite())
			return {};
		const digit_t* const digits = DigitsPointer();
		return array_t<const digit_t>::FromUnsafePointer(digits, digits == nullptr ? 0 : n_decimal);
	}

	/*****************************************************************/
	// MATH OPERATIONS

	int TBCD::AbsAdd(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(out.base != lhs.base || out.base != rhs.base, TLogicException);
		const unsigned radix = out.Radix();
		const ssys_t low = -(ssys_t)util::Max(out.n_decimal, util::Max(lhs.n_decimal, rhs.n_decimal));
		const ssys_t high = (ssys_t)util::Max(out.n_integer, util::Max(lhs.n_integer, rhs.n_integer));
		const ssys_t out_low = -(ssys_t)out.n_decimal;
		const ssys_t out_high = (ssys_t)out.n_integer;
		unsigned carry = 0;
		bool overflow = false;

		out.is_zero = 0;
		for(ssys_t i = low; i < high; i++)
		{
			unsigned value = (unsigned)lhs.Digit(i) + (unsigned)rhs.Digit(i) + carry;
			carry = value / radix;
			value %= radix;

			if(i >= out_low && i < out_high)
				out.Digit(i, (digit_t)value);
			else if(i >= out_high && value != 0)
				overflow = true;
		}

		if(carry != 0)
			overflow = true;
		(void)out.IsZero();
		return overflow ? 1 : 0;
	}

	int TBCD::AbsSub(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(out.base != lhs.base || out.base != rhs.base, TLogicException);
		EL_ERROR(CompareMagnitude(lhs, rhs) < 0, TLogicException);
		const unsigned radix = out.Radix();
		const ssys_t low = -(ssys_t)util::Max(out.n_decimal, util::Max(lhs.n_decimal, rhs.n_decimal));
		const ssys_t high = (ssys_t)util::Max(out.n_integer, util::Max(lhs.n_integer, rhs.n_integer));
		const ssys_t out_low = -(ssys_t)out.n_decimal;
		const ssys_t out_high = (ssys_t)out.n_integer;
		unsigned borrow = 0;
		bool overflow = false;

		out.is_zero = 0;
		for(ssys_t i = low; i < high; i++)
		{
			int value = (int)lhs.Digit(i) - (int)rhs.Digit(i) - (int)borrow;
			if(value < 0)
			{
				value += (int)radix;
				borrow = 1;
			}
			else
			{
				borrow = 0;
			}

			if(i >= out_low && i < out_high)
				out.Digit(i, (digit_t)value);
			else if(i >= out_high && value != 0)
				overflow = true;
		}

		EL_ERROR(borrow != 0, TLogicException);
		(void)out.IsZero();
		return overflow ? 1 : 0;
	}

	void TBCD::AbsMul(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(out.base != lhs.base || out.base != rhs.base, TLogicException);
		if(lhs.IsZero() || rhs.IsZero())
		{
			out.SetZero();
			return;
		}

		const unsigned radix = out.Radix();
		TList<digit_t> left(lhs.Digits());
		TList<digit_t> right(rhs.Digits());
		detail::TrimDigits(left);
		detail::TrimDigits(right);
		TList<digit_t> product;
		detail::MultiplyDigits(product, left, right, radix);
		const usys_t source_decimal = lhs.n_decimal + rhs.n_decimal;
		if(out.n_decimal >= source_decimal)
			detail::ShiftDigitsLeft(product, out.n_decimal - source_decimal);
		else
			detail::DivideDigitsPower(product, radix, radix, source_decimal - out.n_decimal);
		AssignMagnitudeDigits(out, product, false);
	}

	int TBCD::Add(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(lhs.IsInvalid(), TInvalidArgumentException, "lhs", "value is not valid");
		EL_ERROR(rhs.IsInvalid(), TInvalidArgumentException, "rhs", "value is not valid");
		EL_ERROR(out.IsInvalid(), TInvalidArgumentException, "out", "value is not valid");
		if(lhs.IsNaN() || rhs.IsNaN())
		{
			out.SetNaN();
			return 0;
		}
		if(lhs.IsInfinity() || rhs.IsInfinity())
		{
			if(lhs.IsInfinity() && rhs.IsInfinity() && lhs.IsNegative() != rhs.IsNegative())
				out.SetNaN();
			else
				out.SetInfinity(lhs.IsInfinity() ? lhs.IsNegative() : rhs.IsNegative());
			return 0;
		}
		if(!out.IsFinite())
			out.SetZero();
		out.is_periodic = 0;

		if(out.base != lhs.base || out.base != rhs.base)
			return AddGeneric(out, lhs, rhs, false);

		const bool lhs_negative = lhs.is_negative && !lhs.IsZero();
		const bool rhs_negative = rhs.is_negative && !rhs.IsZero();
		if(lhs_negative == rhs_negative)
		{
			const int carry = AbsAdd(out, lhs, rhs);
			out.is_negative = lhs_negative;
			return carry;
		}

		const int cmp = CompareMagnitude(lhs, rhs);
		if(cmp == 0)
		{
			out.SetZero();
			return 0;
		}

		if(cmp > 0)
		{
			const int carry = AbsSub(out, lhs, rhs);
			out.is_negative = lhs_negative;
			return carry;
		}

		const int carry = AbsSub(out, rhs, lhs);
		out.is_negative = rhs_negative;
		return carry;
	}

	int TBCD::Subtract(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(lhs.IsInvalid(), TInvalidArgumentException, "lhs", "value is not valid");
		EL_ERROR(rhs.IsInvalid(), TInvalidArgumentException, "rhs", "value is not valid");
		EL_ERROR(out.IsInvalid(), TInvalidArgumentException, "out", "value is not valid");
		if(lhs.IsNaN() || rhs.IsNaN())
		{
			out.SetNaN();
			return 0;
		}
		if(lhs.IsInfinity() || rhs.IsInfinity())
		{
			if(lhs.IsInfinity() && rhs.IsInfinity() && lhs.IsNegative() == rhs.IsNegative())
				out.SetNaN();
			else if(lhs.IsInfinity())
				out.SetInfinity(lhs.IsNegative());
			else
				out.SetInfinity(!rhs.IsNegative());
			return 0;
		}
		if(!out.IsFinite())
			out.SetZero();
		out.is_periodic = 0;

		if(out.base != lhs.base || out.base != rhs.base)
			return AddGeneric(out, lhs, rhs, true);

		const bool lhs_negative = lhs.is_negative && !lhs.IsZero();
		const bool rhs_negative = rhs.is_negative && !rhs.IsZero();
		if(lhs_negative != rhs_negative)
		{
			const int carry = AbsAdd(out, lhs, rhs);
			out.is_negative = lhs_negative;
			return carry;
		}

		const int cmp = CompareMagnitude(lhs, rhs);
		if(cmp == 0)
		{
			out.SetZero();
			return 0;
		}

		if(cmp > 0)
		{
			const int carry = AbsSub(out, lhs, rhs);
			out.is_negative = lhs_negative;
			return carry;
		}

		const int carry = AbsSub(out, rhs, lhs);
		out.is_negative = !rhs_negative;
		return carry;
	}

	void TBCD::Multiply(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(lhs.IsInvalid(), TInvalidArgumentException, "lhs", "value is not valid");
		EL_ERROR(rhs.IsInvalid(), TInvalidArgumentException, "rhs", "value is not valid");
		EL_ERROR(out.IsInvalid(), TInvalidArgumentException, "out", "value is not valid");
		if(lhs.IsNaN() || rhs.IsNaN() || ((lhs.IsInfinity() && rhs.IsZero()) || (rhs.IsInfinity() && lhs.IsZero())))
		{
			out.SetNaN();
			return;
		}
		if(lhs.IsInfinity() || rhs.IsInfinity())
		{
			out.SetInfinity(lhs.IsNegative() != rhs.IsNegative());
			return;
		}
		if(!out.IsFinite())
			out.SetZero();
		out.is_periodic = 0;

		if(lhs.IsZero() || rhs.IsZero())
		{
			out.SetZero();
			return;
		}

		const bool negative = lhs.is_negative != rhs.is_negative;
		if(out.base == lhs.base && out.base == rhs.base)
			AbsMul(out, lhs, rhs);
		else
			MultiplyGeneric(out, lhs, rhs);
		out.is_negative = negative;
	}

	TBCD TBCD::Divide(TBCD& out, const TBCD& lhs, const TBCD& rhs)
	{
		EL_ERROR(lhs.IsInvalid(), TInvalidArgumentException, "lhs", "value is not valid");
		EL_ERROR(rhs.IsInvalid(), TInvalidArgumentException, "rhs", "value is not valid");
		EL_ERROR(out.IsInvalid(), TInvalidArgumentException, "out", "value is not valid");
		if(lhs.IsNaN() || rhs.IsNaN())
		{
			out.SetNaN();
			TBCD remainder(0, out);
			remainder.SetNaN();
			return remainder;
		}
		if(lhs.IsInfinity() && rhs.IsInfinity())
		{
			out.SetNaN();
			TBCD remainder(0, out);
			remainder.SetNaN();
			return remainder;
		}
		if(rhs.IsInfinity())
		{
			TBCD remainder(lhs, out);
			out.SetZero();
			return remainder;
		}
		if(lhs.IsInfinity())
		{
			out.SetInfinity(lhs.IsNegative() != rhs.IsNegative());
			TBCD remainder(0, out);
			remainder.SetNaN();
			return remainder;
		}
		if(rhs.IsZero())
		{
			TBCD remainder(0, out);
			if(lhs.IsZero())
				out.SetNaN();
			else
				out.SetInfinity(lhs.IsNegative() != rhs.IsNegative());
			remainder.SetNaN();
			return remainder;
		}
		if(!out.IsFinite())
			out.SetZero();
		out.is_periodic = 0;
		return DivideGeneric(out, lhs, rhs);
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
		*this = Divide(*this, *this, rhs);
		return *this;
	}

	TBCD& TBCD::operator<<=(const unsigned n_shift)
	{
		if(!IsFinite())
			return *this;
		if(n_shift == 0 || IsZero())
			return *this;

		const usys_t n_digits = n_decimal + n_integer;
		if(n_shift >= n_digits)
		{
			SetZero();
			return *this;
		}

		digit_t* const digits = DigitsPointer();
		memmove(digits + n_shift, digits, n_digits - n_shift);
		memset(digits, 0, n_shift);
		is_zero = 0;
		(void)IsZero();
		return *this;
	}

	TBCD& TBCD::operator>>=(const unsigned n_shift)
	{
		if(!IsFinite())
			return *this;
		if(n_shift == 0 || IsZero())
			return *this;

		const usys_t n_digits = n_decimal + n_integer;
		if(n_shift >= n_digits)
		{
			SetZero();
			return *this;
		}

		digit_t* const digits = DigitsPointer();
		memmove(digits, digits + n_shift, n_digits - n_shift);
		memset(digits + n_digits - n_shift, 0, n_shift);
		is_zero = 0;
		(void)IsZero();
		return *this;
	}

	/*****************************************************************/
	// COMPARISON

	int TBCD::Compare(const TBCD& rhs, const bool absolute) const
	{
		EL_ERROR(IsInvalid(), TInvalidArgumentException, "this", "value is not valid");
		EL_ERROR(rhs.IsInvalid(), TInvalidArgumentException, "rhs", "value is not valid");
		EL_ERROR(IsNaN() || rhs.IsNaN(), TInvalidArgumentException, "rhs", "comparison with NaN is unordered");

		if(IsInfinity() || rhs.IsInfinity())
		{
			if(IsInfinity() && rhs.IsInfinity())
			{
				if(absolute)
					return 0;
				if(IsNegative() == rhs.IsNegative())
					return 0;
				return IsNegative() ? -1 : 1;
			}
			if(absolute)
				return IsInfinity() ? 1 : -1;
			if(IsInfinity())
				return IsNegative() ? -1 : 1;
			return rhs.IsNegative() ? 1 : -1;
		}

		const bool lhs_zero = IsZero();
		const bool rhs_zero = rhs.IsZero();
		if(lhs_zero && rhs_zero)
			return 0;

		const bool lhs_negative = is_negative && !lhs_zero;
		const bool rhs_negative = rhs.is_negative && !rhs_zero;
		if(!absolute && lhs_negative != rhs_negative)
			return lhs_negative ? -1 : 1;

		int result = CompareMagnitude(*this, rhs);
		if(!absolute && lhs_negative)
			result = -result;
		return result;
	}

	bool TBCD::HasSameSpecs(const TBCD& rhs) const
	{
		return base == rhs.base && n_integer == rhs.n_integer && n_decimal == rhs.n_decimal;
	}

	/*****************************************************************/
	// UTILITY FUNCTIONS

	usys_t TBCD::RequiredDigits(const digit_t target_base, const digit_t source_base, const usys_t n_source_digits)
	{
		EL_ERROR(target_base == 1, TInvalidArgumentException, "target_base", "base 1 is reserved for invalid values");
		EL_ERROR(source_base == 1, TInvalidArgumentException, "source_base", "base 1 is reserved for invalid values");
		if(n_source_digits == 0)
			return 0;

		const unsigned source_radix = source_base == 0 ? 256U : (unsigned)source_base;
		const unsigned target_radix = target_base == 0 ? 256U : (unsigned)target_base;
		if(source_radix == target_radix)
			return n_source_digits;

		const long double source_log = std::log((long double)source_radix);
		const long double target_log = std::log((long double)target_radix);
		const long double required = (long double)n_source_digits * source_log / target_log;
		EL_ERROR(!std::isfinite(required) || required > (long double)std::numeric_limits<usys_t>::max(), TInvalidArgumentException, "n_source_digits", "result digit count exceeds addressable memory");

		// source_radix^n - 1 needs ceil(log_target(source_radix^n)) digits.
		// For the small supported radices long double has ample precision for all
		// practically addressable buffers. Exact power relations naturally produce
		// an integral value here and therefore do not gain an extra digit.
		const long double rounded_up = std::ceil(required);
		return (usys_t)rounded_up;
	}

	std::tuple<ssys_t,ssys_t> TBCD::OuterBounds(const TBCD& rhs) const
	{
		return {
			-(ssys_t)util::Max(n_decimal, rhs.n_decimal),
			 (ssys_t)util::Max(n_integer, rhs.n_integer) - 1
		};
	}

	std::tuple<ssys_t,ssys_t> TBCD::InnerBounds(const TBCD& rhs) const
	{
		return {
			-(ssys_t)util::Min(n_decimal, rhs.n_decimal),
			 (ssys_t)util::Min(n_integer, rhs.n_integer) - 1
		};
	}

	void TBCD::Shift(const ssys_t shift)
	{
		if(!IsFinite())
			return;
		if(shift > 0)
			(*this) <<= (unsigned)shift;
		else if(shift < 0)
		{
			const unsigned magnitude = (unsigned)(-(shift + 1)) + 1U;
			(*this) >>= magnitude;
		}
	}

	void TBCD::Round(const usys_t n_decimal_max, const ERoundingMode mode)
	{
		if(!IsFinite())
			return;
		if(n_decimal_max >= n_decimal || IsZero())
			return;
		is_periodic = 0;

		const usys_t n_discard = n_decimal - n_decimal_max;
		const digit_t* const digits = DigitsPointer();
		bool has_discarded_value = false;
		for(usys_t i = 0; i < n_discard; i++)
			if(digits[i] != 0)
			{
				has_discarded_value = true;
				break;
			}

		if(!has_discarded_value)
			return;

		const unsigned radix = Radix();
		bool increment = false;
		switch(mode)
		{
			case ERoundingMode::TOWARDS_ZERO:
				increment = false;
				break;

			case ERoundingMode::AWAY_FROM_ZERO:
				increment = true;
				break;

			case ERoundingMode::DOWNWARD:
				increment = is_negative;
				break;

			case ERoundingMode::UPWARD:
				increment = !is_negative;
				break;

			case ERoundingMode::TO_NEAREST:
			case ERoundingMode::TO_NEAREST_EVEN:
			{
				// The most significant discarded digit decides against 1/2 immediately
				// unless an even radix represents exactly radix/2. Lower discarded
				// digits only distinguish an exact tie from a value above the tie.
				const unsigned leading = digits[n_discard - 1U];
				const unsigned twice_leading = leading * 2U;
				if(twice_leading > radix)
				{
					increment = true;
				}
				else if(twice_leading == radix)
				{
					bool has_lower_digits = false;
					for(usys_t i = 0; i + 1U < n_discard; i++)
						if(digits[i] != 0)
						{
							has_lower_digits = true;
							break;
						}

					if(has_lower_digits || mode == ERoundingMode::TO_NEAREST)
						increment = true;
					else
					{
						const ssys_t retained_index = -(ssys_t)n_decimal_max;
						increment = (Digit(retained_index) & 1U) != 0;
					}
				}
				break;
			}

			case ERoundingMode::STOCHASTIC:
			{
				for(usys_t i = n_discard; i > 0; i--)
				{
					const digit_t random_digit = RandomDigit(radix);
					const digit_t remainder_digit = digits[i - 1];
					if(random_digit < remainder_digit)
					{
						increment = true;
						break;
					}
					if(random_digit > remainder_digit)
						break;
				}
				break;
			}
		}

		memset(DigitsPointer(), 0, n_discard);
		if(increment)
		{
			unsigned carry = 1;
			for(unsigned i = n_discard; carry != 0 && i < n_decimal + n_integer; i++)
			{
				unsigned value = (unsigned)DigitsPointer()[i] + carry;
				carry = value / radix;
				DigitsPointer()[i] = (digit_t)(value % radix);
			}
		}

		is_zero = 0;
		(void)IsZero();
	}

	bool TBCD::SetPrecision(const usys_t new_n_integer, const usys_t new_n_decimal, const ERoundingMode mode)
	{
		EL_ERROR(IsInvalid(), TInvalidArgumentException, "this", "not valid");
		const usys_t new_count = detail::CheckedDigitCount(new_n_integer, new_n_decimal);
		if(new_n_integer == n_integer && new_n_decimal == n_decimal)
			return false;
		if(!IsFinite())
		{
			n_integer = detail::CheckedPrecision(new_n_integer, "new_n_integer");
			n_decimal = detail::CheckedPrecision(new_n_decimal, "new_n_decimal");
			return false;
		}

		if(new_n_decimal < n_decimal)
			Round(new_n_decimal, mode);

		bool overflow = false;
		for(usys_t i = new_n_integer; i < n_integer; i++)
			if(Digit((ssys_t)i) != 0)
			{
				overflow = true;
				break;
			}

		TList<digit_t> new_digits;
		if(!IsZero() && new_count != 0)
		{
			new_digits.SetCount(new_count);
			for(usys_t i = 0; i < new_count; i++)
			{
				const ssys_t logical_index = (ssys_t)i - (ssys_t)new_n_decimal;
				new_digits[i] = Digit(logical_index);
			}
		}

		n_integer = detail::CheckedPrecision(new_n_integer, "new_n_integer");
		n_decimal = detail::CheckedPrecision(new_n_decimal, "new_n_decimal");
		digits = std::move(new_digits);
		is_periodic = 0;
		is_zero = 0;
		(void)IsZero();
		return overflow;
	}

	void TBCD::SetZero() noexcept
	{
		if(IsInvalid())
		{
			is_negative = 0;
			is_zero = 1;
			is_periodic = 0;
			value_class = (u8_t)EValueClass::FINITE;
			return;
		}

		digit_t* const digits = DigitsPointer();
		if(digits != nullptr && !is_zero)
			memset(digits, 0, n_decimal + n_integer);
		is_negative = 0;
		is_zero = 1;
		is_periodic = 0;
		value_class = (u8_t)EValueClass::FINITE;
	}

	void TBCD::SetNaN() noexcept
	{
		if(IsInvalid())
			return;
		digits.Clear();
		is_negative = 0;
		is_zero = 0;
		is_periodic = 0;
		value_class = (u8_t)EValueClass::NOT_A_NUMBER;
	}

	void TBCD::SetInfinity(const bool negative) noexcept
	{
		if(IsInvalid())
			return;
		digits.Clear();
		is_negative = negative;
		is_zero = 0;
		is_periodic = 0;
		value_class = (u8_t)EValueClass::INFINITE;
	}

	bool TBCD::IsZero() const noexcept
	{
		if(IsInvalid())
			return true;
		if(!IsFinite())
			return false;
		if(is_zero)
			return true;

		const digit_t* const digits = DigitsPointer();
		if(digits == nullptr)
		{
			is_zero = 1;
			return true;
		}

		for(usys_t i = 0; i < n_integer + n_decimal; i++)
			if(digits[i] != 0)
				return false;

		is_zero = 1;
		return true;
	}

	double TBCD::ToDouble() const
	{
		if(IsNaN())
			return std::numeric_limits<double>::quiet_NaN();
		if(IsInfinity())
			return is_negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
		if(IsZero())
			return 0;

		const unsigned radix = Radix();
		long double value = 0;
		const auto digits = Digits();
		for(usys_t i = digits.Count(); i > 0; i--)
			value = value * radix + digits[i - 1];
		for(usys_t i = 0; i < n_decimal; i++)
			value /= radix;
		if(is_negative)
			value = -value;
		return (double)value;
	}

	s64_t TBCD::ToSignedInt() const
	{
		EL_ERROR(!IsFinite(), TInvalidArgumentException, "this", "special BCD value cannot be converted to an integer");
		u64_t magnitude = ToUnsignedInt();
		if(is_negative && magnitude != 0)
			magnitude = 0U - magnitude;
		return std::bit_cast<s64_t>(magnitude);
	}

	u64_t TBCD::ToUnsignedInt() const
	{
		EL_ERROR(!IsFinite(), TInvalidArgumentException, "this", "special BCD value cannot be converted to an integer");
		if(IsZero())
			return 0;

		u64_t value = 0;
		const unsigned radix = Radix();
		const auto digits = IntegerDigits();
		for(usys_t i = digits.Count(); i > 0; i--)
			value = value * radix + digits[i - 1];
		return value;
	}

	TBCD TBCD::ToBCDInt() const
	{
		return TBCD(*this, base, n_integer, 0);
	}

	usys_t TBCD::CountLeadingZeros() const
	{
		EL_ERROR(!IsFinite(), TInvalidArgumentException, "this", "special BCD value has no digits");
		const digit_t* const digits = DigitsPointer();
		if(digits != nullptr)
			for(usys_t i = 0; i < n_integer; i++)
				if(digits[n_decimal + n_integer - i - 1U] != 0)
					return i;
		return n_integer;
	}

	usys_t TBCD::CountTrailingZeros() const
	{
		EL_ERROR(!IsFinite(), TInvalidArgumentException, "this", "special BCD value has no digits");
		const digit_t* const digits = DigitsPointer();
		if(digits != nullptr)
			for(usys_t i = 0; i < n_decimal; i++)
				if(digits[i] != 0)
					return i;
		return n_decimal;
	}

	ssys_t TBCD::IndexMostSignificantNonZeroDigit() const
	{
		for(ssys_t i = (ssys_t)n_integer - 1; i >= -(ssys_t)n_decimal; i--)
			if(Digit(i) != 0)
				return i;
		EL_THROW(TInvalidArgumentException, "*this", "cannot have a zero value");
	}

	digit_t TBCD::Digit(const ssys_t index) const
	{
		EL_ERROR(!IsFinite(), TInvalidArgumentException, "this", "special BCD value has no digits");
		if(is_zero)
			return 0;

		const ssys_t i = index + (ssys_t)n_decimal;
		if(i < 0 || (usys_t)i >= n_decimal + n_integer)
			return 0;

		const digit_t* const digits = DigitsPointer();
		return digits == nullptr ? 0 : digits[i];
	}

	bool TBCD::Digit(const ssys_t index, const digit_t digit)
	{
		EL_ERROR(!IsFinite(), TInvalidArgumentException, "this", "special BCD value has no digits");
		const ssys_t i = index + (ssys_t)n_decimal;
		if(i < 0 || (usys_t)i >= n_decimal + n_integer)
			return false;
		EL_ERROR((unsigned)digit >= Radix(), TInvalidArgumentException, "d", "digit must be smaller than the numeric base");

		if(digit == 0 && DigitsPointer() == nullptr)
			return true;
		EnsureDigits();
		is_periodic = 0;
		if(digit != 0)
			is_zero = 0;
		DigitsPointer()[i] = digit;
		return true;
	}

	/*****************************************************************/
	// CONSTRUCTORS + ASSIGNMENT

	digit_t* TBCD::DigitsPointer()
	{
		EL_ERROR(IsInvalid(), TInvalidArgumentException, "this", "not valid");
		return digits.Count() == 0 ? nullptr : &digits[0];
	}

	const digit_t* TBCD::DigitsPointer() const
	{
		EL_ERROR(IsInvalid(), TInvalidArgumentException, "this", "not valid");
		return digits.Count() == 0 ? nullptr : &digits[0];
	}

	void TBCD::EnsureDigits()
	{
		if(digits.Count() != 0)
			return;

		const usys_t n_digits = detail::CheckedDigitCount(n_integer, n_decimal);
		if(n_digits != 0)
			digits.SetCount(n_digits);
		is_zero = 1;
	}

	template<typename T>
	void TBCD::ConvertInteger(T value)
	{
		using unsigned_t = std::make_unsigned_t<T>;
		const bool negative = std::is_signed_v<T> && value < 0;
		unsigned_t magnitude;
		if constexpr(std::is_signed_v<T>)
		{
			const unsigned_t bits = (unsigned_t)value;
			magnitude = negative ? (unsigned_t)((unsigned_t)0 - bits) : bits;
		}
		else
		{
			magnitude = value;
		}

		const bool is_nonzero = magnitude != 0;
		SetZero();
		const unsigned radix = Radix();
		for(usys_t i = 0; i < n_integer && magnitude != 0; i++)
		{
			const digit_t digit = (digit_t)(magnitude % radix);
			if(digit != 0)
				Digit((ssys_t)i, digit);
			magnitude /= radix;
		}
		is_negative = negative && is_nonzero;
	}

	void TBCD::ConvertFloatParts(const detail::TBinaryFloatParts& value)
	{
		if(value.value_class == EValueClass::NOT_A_NUMBER)
		{
			SetNaN();
			return;
		}
		if(value.value_class == EValueClass::INFINITE)
		{
			SetInfinity(value.negative);
			return;
		}
		if(value.significand == 0)
		{
			SetZero();
			return;
		}

		// Decode the exact IEEE-754 value as significand * 2^exponent and convert
		// that rational number directly to this BCD scale. No decimal formatting,
		// parsing or floating-point arithmetic is involved.
		TList<digit_t> magnitude = BuildIntegerDigits(value.significand, Radix());
		if(value.exponent > 0)
			detail::MultiplyDigitsPower(magnitude, Radix(), 2U, (usys_t)value.exponent);

		detail::ShiftDigitsLeft(magnitude, n_decimal);
		if(value.exponent < 0)
			detail::DivideDigitsPower(magnitude, Radix(), 2U, (usys_t)-value.exponent);

		AssignMagnitudeDigits(*this, magnitude, value.negative);
		is_periodic = false;
	}

	void TBCD::ConvertFloat(const float value)
	{
		ConvertFloatParts(detail::DecodeBinaryFloat(value));
	}

	void TBCD::ConvertFloat(const double value)
	{
		ConvertFloatParts(detail::DecodeBinaryFloat(value));
	}

	void TBCD::ConvertBCD(const TBCD& value)
	{
		EL_ERROR(value.IsInvalid(), TInvalidArgumentException, "value", "value is not valid");
		if(&value == this)
			return;
		if(value.IsNaN())
		{
			SetNaN();
			return;
		}
		if(value.IsInfinity())
		{
			SetInfinity(value.IsNegative());
			return;
		}

		if(base == value.base)
		{
			const bool negative = value.is_negative;
			const bool periodic = value.is_periodic;
			SetZero();
			for(ssys_t i = -(ssys_t)n_decimal; i < (ssys_t)n_integer; i++)
			{
				const digit_t digit = value.Digit(i);
				if(digit != 0)
					Digit(i, digit);
			}
			is_negative = negative;
			is_periodic = periodic && n_integer == value.n_integer && n_decimal == value.n_decimal;
			return;
		}

		TList<digit_t> converted = BuildMagnitudeDigits(value, Radix());
		detail::ShiftDigitsLeft(converted, n_decimal);
		detail::DivideDigitsPower(converted, Radix(), value.Radix(), value.n_decimal);
		AssignMagnitudeDigits(*this, converted, value.is_negative);
		is_periodic = false;
	}

	TBCD& TBCD::operator=(TBCD&& rhs)
	{
		if(&rhs == this)
			return *this;

		if(HasSameSpecs(rhs))
		{
			digits = std::move(rhs.digits);
			is_negative = rhs.is_negative;
			is_zero = rhs.is_zero;
			is_periodic = rhs.is_periodic;
			value_class = rhs.value_class;
			rhs.is_negative = 0;
			rhs.is_zero = 1;
			rhs.is_periodic = 0;
			rhs.value_class = (u8_t)EValueClass::FINITE;
		}
		else
		{
			ConvertBCD(rhs);
			rhs.SetZero();
		}
		return *this;
	}

	TBCD& TBCD::operator=(const TBCD& rhs)
	{
		if(&rhs != this)
			ConvertBCD(rhs);
		return *this;
	}

	TBCD& TBCD::operator=(const double rhs)
	{
		ConvertFloat(rhs);
		return *this;
	}

	TBCD& TBCD::operator=(const u64_t rhs)
	{
		ConvertInteger(rhs);
		return *this;
	}

	TBCD& TBCD::operator=(const s64_t rhs)
	{
		ConvertInteger(rhs);
		return *this;
	}

	TBCD& TBCD::operator=(const int rhs)
	{
		ConvertInteger(rhs);
		return *this;
	}

	TBCD::TBCD(const float value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer, "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(n_integer, n_decimal);
			ConvertFloat(value);
		}
	}

	TBCD::TBCD(const double value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer, "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(n_integer, n_decimal);
			ConvertFloat(value);
		}
	}

	TBCD::TBCD(const TBCD& value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) :
		digits(),
		n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, value.base, value.n_integer), "n_integer")),
		n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : RequiredDigits(base, value.base, value.n_decimal), "n_decimal")),
		base(base),
		is_negative(0),
		is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertBCD(value);
		}
	}

	TBCD::TBCD(const u8_t value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(value) * 8U), "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : 0, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertInteger(value);
		}
	}

	TBCD::TBCD(const s8_t value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(value) * 8U), "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : 0, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertInteger(value);
		}
	}

	TBCD::TBCD(const u16_t value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(value) * 8U), "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : 0, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertInteger(value);
		}
	}

	TBCD::TBCD(const s16_t value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(value) * 8U), "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : 0, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertInteger(value);
		}
	}

	TBCD::TBCD(const u32_t value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(value) * 8U), "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : 0, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertInteger(value);
		}
	}

	TBCD::TBCD(const s32_t value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(value) * 8U), "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : 0, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertInteger(value);
		}
	}

	TBCD::TBCD(const u64_t value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(value) * 8U), "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : 0, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertInteger(value);
		}
	}

	TBCD::TBCD(const s64_t value, const digit_t base, const usys_t n_integer, const usys_t n_decimal) : digits(), n_integer(detail::CheckedPrecision(n_integer != AUTO_DETECT ? n_integer : RequiredDigits(base, 2, sizeof(value) * 8U), "n_integer")), n_decimal(detail::CheckedPrecision(n_decimal != AUTO_DETECT ? n_decimal : 0, "n_decimal")), base(base), is_negative(0), is_zero(1)
	{
		if(base != 1)
		{
			detail::CheckedDigitCount(this->n_integer, this->n_decimal);
			ConvertInteger(value);
		}
	}

	TBCD::TBCD(TBCD value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertBCD(value); }
	TBCD::TBCD(const u8_t value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertInteger(value); }
	TBCD::TBCD(const s8_t value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertInteger(value); }
	TBCD::TBCD(const u16_t value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertInteger(value); }
	TBCD::TBCD(const s16_t value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertInteger(value); }
	TBCD::TBCD(const u32_t value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertInteger(value); }
	TBCD::TBCD(const s32_t value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertInteger(value); }
	TBCD::TBCD(const u64_t value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertInteger(value); }
	TBCD::TBCD(const s64_t value, const TBCD& conf_ref) : TBCD(0.0, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) { ConvertInteger(value); }
	TBCD::TBCD(const float value, const TBCD& conf_ref) : TBCD(value, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}
	TBCD::TBCD(const double value, const TBCD& conf_ref) : TBCD(value, conf_ref.base, conf_ref.n_integer, conf_ref.n_decimal) {}

	TBCD::TBCD(const TBCD& value) : digits(value.digits), n_integer(value.n_integer), n_decimal(value.n_decimal), base(value.base), is_negative(value.is_negative), is_zero(value.is_zero), is_periodic(value.is_periodic), value_class(value.value_class)
	{
	}

	TBCD::TBCD(TBCD&& rhs) noexcept : digits(std::move(rhs.digits)), n_integer(rhs.n_integer), n_decimal(rhs.n_decimal), base(rhs.base), is_negative(rhs.is_negative), is_zero(rhs.is_zero), is_periodic(rhs.is_periodic), value_class(rhs.value_class)
	{
		rhs.is_negative = 0;
		rhs.is_zero = 1;
		rhs.is_periodic = 0;
		rhs.value_class = (u8_t)EValueClass::FINITE;
	}

	/*****************************************************************/
	// WRAPPER FUNCTIONS

	TBCD& TBCD::operator+=(const double rhs) { return (*this) += TBCD(rhs, *this); }
	TBCD& TBCD::operator-=(const double rhs) { return (*this) -= TBCD(rhs, *this); }
	TBCD& TBCD::operator*=(const double rhs) { return (*this) *= TBCD(rhs, *this); }
	TBCD& TBCD::operator/=(const double rhs) { return (*this) /= TBCD(rhs, *this); }
	TBCD& TBCD::operator%=(const double rhs) { return (*this) %= TBCD(rhs, *this); }
	TBCD& TBCD::operator+=(const u64_t rhs) { return (*this) += TBCD(rhs, *this); }
	TBCD& TBCD::operator-=(const u64_t rhs) { return (*this) -= TBCD(rhs, *this); }
	TBCD& TBCD::operator*=(const u64_t rhs) { return (*this) *= TBCD(rhs, *this); }
	TBCD& TBCD::operator/=(const u64_t rhs) { return (*this) /= TBCD(rhs, *this); }
	TBCD& TBCD::operator%=(const u64_t rhs) { return (*this) %= TBCD(rhs, *this); }
	TBCD& TBCD::operator+=(const s64_t rhs) { return (*this) += TBCD(rhs, *this); }
	TBCD& TBCD::operator-=(const s64_t rhs) { return (*this) -= TBCD(rhs, *this); }
	TBCD& TBCD::operator*=(const s64_t rhs) { return (*this) *= TBCD(rhs, *this); }
	TBCD& TBCD::operator/=(const s64_t rhs) { return (*this) /= TBCD(rhs, *this); }
	TBCD& TBCD::operator%=(const s64_t rhs) { return (*this) %= TBCD(rhs, *this); }
	TBCD& TBCD::operator+=(const int rhs) { return (*this) += TBCD(rhs, *this); }
	TBCD& TBCD::operator-=(const int rhs) { return (*this) -= TBCD(rhs, *this); }
	TBCD& TBCD::operator*=(const int rhs) { return (*this) *= TBCD(rhs, *this); }
	TBCD& TBCD::operator/=(const int rhs) { return (*this) /= TBCD(rhs, *this); }
	TBCD& TBCD::operator%=(const int rhs) { return (*this) %= TBCD(rhs, *this); }

	TBCD TBCD::operator+(const TBCD& rhs) const { TBCD out(*this); out += rhs; return out; }
	TBCD TBCD::operator-(const TBCD& rhs) const { TBCD out(*this); out -= rhs; return out; }
	TBCD TBCD::operator*(const TBCD& rhs) const { TBCD out(*this); out *= rhs; return out; }
	TBCD TBCD::operator/(const TBCD& rhs) const { TBCD out(*this); out /= rhs; return out; }
	TBCD TBCD::operator%(const TBCD& rhs) const { TBCD out(*this); out %= rhs; return out; }
	TBCD TBCD::operator+(const double rhs) const { return (*this) + TBCD(rhs, *this); }
	TBCD TBCD::operator-(const double rhs) const { return (*this) - TBCD(rhs, *this); }
	TBCD TBCD::operator*(const double rhs) const { return (*this) * TBCD(rhs, *this); }
	TBCD TBCD::operator/(const double rhs) const { return (*this) / TBCD(rhs, *this); }
	TBCD TBCD::operator%(const double rhs) const { return (*this) % TBCD(rhs, *this); }
	TBCD TBCD::operator+(const u64_t rhs) const { return (*this) + TBCD(rhs, *this); }
	TBCD TBCD::operator-(const u64_t rhs) const { return (*this) - TBCD(rhs, *this); }
	TBCD TBCD::operator*(const u64_t rhs) const { return (*this) * TBCD(rhs, *this); }
	TBCD TBCD::operator/(const u64_t rhs) const { return (*this) / TBCD(rhs, *this); }
	TBCD TBCD::operator%(const u64_t rhs) const { return (*this) % TBCD(rhs, *this); }
	TBCD TBCD::operator+(const s64_t rhs) const { return (*this) + TBCD(rhs, *this); }
	TBCD TBCD::operator-(const s64_t rhs) const { return (*this) - TBCD(rhs, *this); }
	TBCD TBCD::operator*(const s64_t rhs) const { return (*this) * TBCD(rhs, *this); }
	TBCD TBCD::operator/(const s64_t rhs) const { return (*this) / TBCD(rhs, *this); }
	TBCD TBCD::operator%(const s64_t rhs) const { return (*this) % TBCD(rhs, *this); }
	TBCD TBCD::operator+(const int rhs) const { return (*this) + TBCD(rhs, *this); }
	TBCD TBCD::operator-(const int rhs) const { return (*this) - TBCD(rhs, *this); }
	TBCD TBCD::operator*(const int rhs) const { return (*this) * TBCD(rhs, *this); }
	TBCD TBCD::operator/(const int rhs) const { return (*this) / TBCD(rhs, *this); }
	TBCD TBCD::operator%(const int rhs) const { return (*this) % TBCD(rhs, *this); }

	TBCD TBCD::operator<<(const unsigned n_shift) const { TBCD out(*this); out <<= n_shift; return out; }
	TBCD TBCD::operator>>(const unsigned n_shift) const { TBCD out(*this); out >>= n_shift; return out; }

	bool TBCD::operator==(const TBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) == 0; }
	bool TBCD::operator!=(const TBCD& rhs) const { return IsNaN() || rhs.IsNaN() || Compare(rhs) != 0; }
	bool TBCD::operator>=(const TBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) >= 0; }
	bool TBCD::operator<=(const TBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) <= 0; }
	bool TBCD::operator> (const TBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) > 0; }
	bool TBCD::operator< (const TBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) < 0; }

	bool TBCD::operator==(const double rhs) const { return CompareFloating(rhs) == 0; }
	bool TBCD::operator!=(const double rhs) const { return CompareFloating(rhs) != 0; }
	bool TBCD::operator>=(const double rhs) const { const int c = CompareFloating(rhs); return c != 2 && c >= 0; }
	bool TBCD::operator<=(const double rhs) const { const int c = CompareFloating(rhs); return c != 2 && c <= 0; }
	bool TBCD::operator> (const double rhs) const { return CompareFloating(rhs) == 1; }
	bool TBCD::operator< (const double rhs) const { return CompareFloating(rhs) == -1; }

	bool TBCD::operator==(const u64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) == 0; }
	bool TBCD::operator!=(const u64_t rhs) const { return IsNaN() || Compare(TBCD(rhs, base)) != 0; }
	bool TBCD::operator>=(const u64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) >= 0; }
	bool TBCD::operator<=(const u64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) <= 0; }
	bool TBCD::operator> (const u64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) > 0; }
	bool TBCD::operator< (const u64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) < 0; }
	bool TBCD::operator==(const s64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) == 0; }
	bool TBCD::operator!=(const s64_t rhs) const { return IsNaN() || Compare(TBCD(rhs, base)) != 0; }
	bool TBCD::operator>=(const s64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) >= 0; }
	bool TBCD::operator<=(const s64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) <= 0; }
	bool TBCD::operator> (const s64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) > 0; }
	bool TBCD::operator< (const s64_t rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) < 0; }
	bool TBCD::operator==(const int rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) == 0; }
	bool TBCD::operator!=(const int rhs) const { return IsNaN() || Compare(TBCD(rhs, base)) != 0; }
	bool TBCD::operator>=(const int rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) >= 0; }
	bool TBCD::operator<=(const int rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) <= 0; }
	bool TBCD::operator> (const int rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) > 0; }
	bool TBCD::operator< (const int rhs) const { return !IsNaN() && Compare(TBCD(rhs, base)) < 0; }

	TBCD::operator double() const { return ToDouble(); }
	TBCD::operator s64_t() const { return ToSignedInt(); }
	TBCD::operator u64_t() const { return ToUnsignedInt(); }

	TBCD TBCD::FromString(const text::string::TStringView str, const text::string::TStringView symbols, const char32_t decimal_seperator, const char32_t negative_symbol, const char32_t positive_symbol, const bool default_negative)
	{
		if(str.Length() == 0)
			return INVALID;

		EL_ERROR(symbols.Length() < 2 || symbols.Length() > 256, TInvalidArgumentException, "symbols", "between 2 and 256 numeric symbols are required");
		EL_ERROR(symbols.Contains(decimal_seperator), TInvalidArgumentException, "symbols", "symbols must not include the decimal separator");
		EL_ERROR(symbols.Contains(negative_symbol), TInvalidArgumentException, "symbols", "symbols must not include the negative sign");
		EL_ERROR(symbols.Contains(positive_symbol), TInvalidArgumentException, "symbols", "symbols must not include the positive sign");
		EL_ERROR(decimal_seperator == negative_symbol || decimal_seperator == positive_symbol || negative_symbol == positive_symbol, TInvalidArgumentException, "symbols", "separator and sign characters must be distinct");

		for(usys_t i = 0; i < symbols.Length(); i++)
			for(usys_t j = i + 1U; j < symbols.Length(); j++)
				EL_ERROR(symbols[i] == symbols[j], TInvalidArgumentException, "symbols", "numeric symbols must be unique");

		usys_t begin = 0;
		usys_t end = str.Length();
		bool negative = default_negative;
		const bool sign_front = str[0] == negative_symbol || str[0] == positive_symbol;
		const bool sign_back = str[str.Length() - 1U] == negative_symbol || str[str.Length() - 1U] == positive_symbol;
		EL_ERROR(sign_front && sign_back && str.Length() > 1, TInvalidArgumentException, "str", "numeric string contains multiple sign characters");

		if(sign_front)
		{
			negative = str[0] == negative_symbol;
			begin++;
		}
		else if(sign_back)
		{
			negative = str[str.Length() - 1U] == negative_symbol;
			end--;
		}

		EL_ERROR(begin >= end, TInvalidArgumentException, "str", "numeric string contains no digits");

		usys_t decimal = NEG1;
		unsigned n_digits = 0;
		for(usys_t i = begin; i < end; i++)
		{
			if(str[i] == decimal_seperator)
			{
				EL_ERROR(decimal != NEG1, TInvalidArgumentException, "str", "numeric string contains multiple decimal separators");
				decimal = i;
			}
			else
			{
				n_digits++;
			}
		}
		EL_ERROR(n_digits == 0, TInvalidArgumentException, "str", "numeric string contains no digits");

		const usys_t n_decimal_digits = decimal == NEG1 ? 0U : decimal - begin;
		const usys_t n_integer_digits = n_digits - n_decimal_digits;
		const digit_t base = symbols.Length() == 256U ? 0 : (digit_t)symbols.Length();
		TBCD result(0, base, n_integer_digits, n_decimal_digits);
		result.EnsureDigits();
		usys_t output = 0;
		for(usys_t i = begin; i < end; i++)
		{
			if(str[i] == decimal_seperator)
				continue;
			const usys_t value = symbols.Find(str[i]);
			EL_ERROR(value == NEG1, TInvalidArgumentException, "str", "numeric string contains a character not present in symbols");
			result.DigitsPointer()[output++] = (digit_t)value;
			if(value != 0)
				result.is_zero = 0;
		}
		result.is_negative = negative;
		return result;
	}

	TBCD TBCD::FromStringMSD(const text::string::TStringView str, const text::string::TStringView symbols, const char32_t decimal_seperator, const char32_t negative_symbol, const char32_t positive_symbol, const bool default_negative)
	{
		if(str.Length() == 0)
			return INVALID;

		EL_ERROR(symbols.Length() < 2 || symbols.Length() > 256, TInvalidArgumentException, "symbols", "between 2 and 256 numeric symbols are required");
		EL_ERROR(symbols.Contains(decimal_seperator), TInvalidArgumentException, "symbols", "symbols must not include the decimal separator");
		EL_ERROR(symbols.Contains(negative_symbol), TInvalidArgumentException, "symbols", "symbols must not include the negative sign");
		EL_ERROR(symbols.Contains(positive_symbol), TInvalidArgumentException, "symbols", "symbols must not include the positive sign");
		EL_ERROR(decimal_seperator == negative_symbol || decimal_seperator == positive_symbol || negative_symbol == positive_symbol, TInvalidArgumentException, "symbols", "separator and sign characters must be distinct");

		for(usys_t i = 0; i < symbols.Length(); i++)
			for(usys_t j = i + 1U; j < symbols.Length(); j++)
				EL_ERROR(symbols[i] == symbols[j], TInvalidArgumentException, "symbols", "numeric symbols must be unique");

		usys_t begin = 0;
		bool negative = default_negative;
		if(str[0] == negative_symbol || str[0] == positive_symbol)
		{
			negative = str[0] == negative_symbol;
			begin = 1;
		}
		EL_ERROR(begin == str.Length(), TInvalidArgumentException, "str", "numeric string contains no digits");

		usys_t decimal = NEG1;
		usys_t n_digits = 0;
		for(usys_t i = begin; i < str.Length(); i++)
		{
			if(str[i] == decimal_seperator)
			{
				EL_ERROR(decimal != NEG1, TInvalidArgumentException, "str", "numeric string contains multiple decimal separators");
				decimal = i;
			}
			else
			{
				n_digits++;
			}
		}
		EL_ERROR(n_digits == 0, TInvalidArgumentException, "str", "numeric string contains no digits");

		const usys_t n_decimal_digits = decimal == NEG1 ? 0U : str.Length() - decimal - 1U;
		const usys_t n_integer_digits = n_digits - n_decimal_digits;
		const digit_t base = symbols.Length() == 256U ? 0 : (digit_t)symbols.Length();
		TBCD result(0, base, n_integer_digits, n_decimal_digits);
		result.EnsureDigits();
		usys_t output = 0;
		for(usys_t i = str.Length(); i > begin; i--)
		{
			const char32_t chr = str[i - 1U];
			if(chr == decimal_seperator)
				continue;
			const usys_t value = symbols.Find(chr);
			EL_ERROR(value == NEG1, TInvalidArgumentException, "str", "numeric string contains a character not present in symbols");
			result.DigitsPointer()[output++] = (digit_t)value;
			if(value != 0)
				result.is_zero = 0;
		}
		result.is_negative = negative;
		return result;
	}

	TBCD TBCD::FromStringMSD(const text::string::TStringView str, const digit_t base, const char32_t decimal_seperator, const char32_t negative_symbol, const char32_t positive_symbol, const bool default_negative)
	{
		const unsigned radix = base == 0 ? 256U : (unsigned)base;
		EL_ERROR(radix < 2 || radix > 36, TInvalidArgumentException, "base", "standard string parsing supports bases 2 through 36");
		EL_ERROR(str.Length() == 0, TInvalidArgumentException, "str", "numeric string contains no digits");
		EL_ERROR(decimal_seperator == negative_symbol || decimal_seperator == positive_symbol || negative_symbol == positive_symbol, TInvalidArgumentException, "str", "separator and sign characters must be distinct");

		auto digit_value = [](const char32_t chr) -> int
		{
			if(chr >= U'0' && chr <= U'9') return (int)(chr - U'0');
			if(chr >= U'a' && chr <= U'z') return 10 + (int)(chr - U'a');
			if(chr >= U'A' && chr <= U'Z') return 10 + (int)(chr - U'A');
			return -1;
		};

		usys_t begin = 0;
		bool negative = default_negative;
		if(str[0] == negative_symbol || str[0] == positive_symbol)
		{
			negative = str[0] == negative_symbol;
			begin = 1;
		}
		EL_ERROR(begin == str.Length(), TInvalidArgumentException, "str", "numeric string contains no digits");

		usys_t decimal = NEG1;
		usys_t n_digits = 0;
		for(usys_t i = begin; i < str.Length(); i++)
		{
			if(str[i] == decimal_seperator)
			{
				EL_ERROR(decimal != NEG1, TInvalidArgumentException, "str", "numeric string contains multiple decimal separators");
				decimal = i;
				continue;
			}

			const int value = digit_value(str[i]);
			EL_ERROR(value < 0 || (unsigned)value >= radix, TInvalidArgumentException, "str", "numeric string contains a digit outside the numeric base");
			n_digits++;
		}
		EL_ERROR(n_digits == 0, TInvalidArgumentException, "str", "numeric string contains no digits");

		const usys_t n_decimal_digits = decimal == NEG1 ? 0U : str.Length() - decimal - 1U;
		const usys_t n_integer_digits = n_digits - n_decimal_digits;
		TBCD result(0, base, n_integer_digits, n_decimal_digits);
		result.EnsureDigits();
		usys_t output = 0;
		for(usys_t i = str.Length(); i > begin; i--)
		{
			const char32_t chr = str[i - 1U];
			if(chr == decimal_seperator)
				continue;
			const digit_t value = (digit_t)digit_value(chr);
			result.DigitsPointer()[output++] = value;
			if(value != 0)
				result.is_zero = 0;
		}
		result.is_negative = negative;
		return result;
	}

	TBCD TBCD::Random(const digit_t base, const usys_t n_integer, const usys_t n_decimal)
	{
		EL_ERROR(base == 1, TInvalidArgumentException, "base", "base 1 is reserved for invalid values");
		TBCD result(0, base, n_integer, n_decimal);
		const usys_t n_digits = n_integer + n_decimal;
		if(n_digits == 0)
			return result;

		result.EnsureDigits();
		auto& rng = system::random::TSystemRandom::Instance();
		digit_t* const digits = result.DigitsPointer();
		const unsigned radix = result.Radix();
		if(radix == 256U)
		{
			rng.ReadAll(digits, n_digits);
		}
		else
		{
			digit_t random_bytes[64];
			usys_t random_pos = sizeof(random_bytes);
			const unsigned limit = 256U - (256U % radix);
			for(usys_t i = 0; i < n_digits; i++)
			{
				digit_t random_byte;
				do
				{
					if(random_pos == sizeof(random_bytes))
					{
						rng.ReadAll(random_bytes, sizeof(random_bytes));
						random_pos = 0;
					}
					random_byte = random_bytes[random_pos++];
				}
				while((unsigned)random_byte >= limit);
				digits[i] = (digit_t)((unsigned)random_byte % radix);
			}
		}

		result.is_zero = 0;
		(void)result.IsZero();
		return result;
	}

	TBCD::TBCD() : TBCD(0, 1, 0, 0) {}
	const TBCD TBCD::INVALID(0, 1, 0, 0);
}
