#pragma once

#include "error.hpp"
#include "math.hpp"
#include "io_types.hpp"
#include "io_collection_array.hpp"
#include "io_collection_list.hpp"
#include "io_text_encoding.hpp"
#include "system_random.hpp"

#include <bit>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace el1::io::text::string
{
	class TString;
	class TStringView;
}

namespace el1::io::collection::list
{
	template<typename T> class TList;
}

namespace el1::io::bcd
{
	using namespace types;
	using digit_t = u8_t;
	static const usys_t AUTO_DETECT = NEG1;
	using precision_t = u16_t;
	static constexpr usys_t MAX_PRECISION = std::numeric_limits<precision_t>::max();

	enum class EValueClass : u8_t
	{
		FINITE,
		INFINITE,
		NOT_A_NUMBER
	};

	namespace detail
	{

		struct TBinaryFloatParts
		{
			u64_t significand = 0;
			ssys_t exponent = 0;
			bool negative = false;
			EValueClass value_class = EValueClass::FINITE;
		};

		template<typename TFloat>
		requires std::is_floating_point_v<TFloat>
		TBinaryFloatParts DecodeBinaryFloat(const TFloat value) noexcept
		{
			static_assert(std::numeric_limits<TFloat>::is_iec559, "BCD floating conversion requires IEC 559 / IEEE-754 floating point");
			static_assert(std::numeric_limits<TFloat>::radix == 2, "BCD floating conversion requires binary floating point");
			static_assert(sizeof(TFloat) == 4 || sizeof(TFloat) == 8, "unsupported floating-point layout");

			using bits_t = std::conditional_t<sizeof(TFloat) == 4, u32_t, u64_t>;
			constexpr unsigned N_BITS = sizeof(TFloat) * CHAR_BIT;
			constexpr unsigned N_FRACTION = std::numeric_limits<TFloat>::digits - 1U;
			constexpr unsigned N_EXPONENT = N_BITS - N_FRACTION - 1U;
			constexpr bits_t EXPONENT_MASK = ((bits_t)1U << N_EXPONENT) - 1U;
			constexpr bits_t FRACTION_MASK = ((bits_t)1U << N_FRACTION) - 1U;
			constexpr ssys_t EXPONENT_BIAS = ((ssys_t)1 << (N_EXPONENT - 1U)) - 1;

			const bits_t bits = std::bit_cast<bits_t>(value);
			const bits_t exponent_bits = (bits >> N_FRACTION) & EXPONENT_MASK;
			const bits_t fraction_bits = bits & FRACTION_MASK;
			TBinaryFloatParts parts;
			parts.negative = (bits >> (N_BITS - 1U)) != 0;

			if(exponent_bits == EXPONENT_MASK)
			{
				parts.value_class = fraction_bits == 0 ? EValueClass::INFINITE : EValueClass::NOT_A_NUMBER;
				return parts;
			}

			if(exponent_bits == 0)
			{
				if(fraction_bits == 0)
					return parts;
				parts.significand = fraction_bits;
				parts.exponent = 1 - EXPONENT_BIAS - (ssys_t)N_FRACTION;
			}
			else
			{
				parts.significand = ((u64_t)1U << N_FRACTION) | (u64_t)fraction_bits;
				parts.exponent = (ssys_t)exponent_bits - EXPONENT_BIAS - (ssys_t)N_FRACTION;
			}

			while((parts.significand & 1U) == 0)
			{
				parts.significand >>= 1U;
				parts.exponent++;
			}
			return parts;
		}
		// Stateless digit-storage algorithms shared by TBCD and TFixedBCD.
		// They carry no sign, radix or scale and are not a numeric type on their own.
		inline precision_t CheckedPrecision(const usys_t value, const char* const argument)
		{
			EL_ERROR(value > MAX_PRECISION, error::TInvalidArgumentException, argument, "precision exceeds u16_t range");
			return (precision_t)value;
		}

		inline usys_t CheckedDigitCount(const usys_t n_integer, const usys_t n_decimal)
		{
			CheckedPrecision(n_integer, "n_integer");
			CheckedPrecision(n_decimal, "n_decimal");
			return n_integer + n_decimal;
		}

		template<typename T, usys_t N>
		class TFixedList
		{
			private:
				T items[N == 0 ? 1 : N] = {};
				usys_t count = 0;

			public:
				constexpr usys_t Count() const noexcept { return count; }
				constexpr T& operator[](const usys_t index) noexcept { return items[index]; }
				constexpr const T& operator[](const usys_t index) const noexcept { return items[index]; }

				void SetCount(const usys_t new_count)
				{
					EL_ERROR(new_count > N, error::TIndexOutOfBoundsException, (ssys_t)0, (ssys_t)N, (ssys_t)new_count);
					if(new_count > count)
						for(usys_t i = count; i < new_count; i++)
							items[i] = T();
					count = new_count;
				}

				void Append(const T& value)
				{
					EL_ERROR(count >= N, error::TIndexOutOfBoundsException, (ssys_t)0, (ssys_t)N - 1, (ssys_t)count);
					items[count++] = value;
				}

				void Truncate() noexcept
				{
					count = 0;
				}

				void Cut(const usys_t n_start, const usys_t n_end)
				{
					EL_ERROR(n_start + n_end > count, error::TIndexOutOfBoundsException, (ssys_t)0, (ssys_t)count, (ssys_t)(n_start + n_end));
					const usys_t n_keep = count - n_start - n_end;
					for(usys_t i = 0; i < n_keep; i++)
						items[i] = items[i + n_start];
					count = n_keep;
				}
		};

		template<typename TBuffer>
		void TrimDigits(TBuffer& digits)
		{
			usys_t n_keep = digits.Count();
			while(n_keep > 0 && digits[n_keep - 1] == 0)
				n_keep--;
			if(n_keep != digits.Count())
				digits.Cut(0, digits.Count() - n_keep);
		}

		template<typename TBuffer>
		bool IsDigitsZero(const TBuffer& digits) noexcept
		{
			return digits.Count() == 0;
		}

		template<typename TBuffer>
		int CompareDigits(const TBuffer& lhs, const TBuffer& rhs) noexcept
		{
			if(lhs.Count() != rhs.Count())
				return lhs.Count() < rhs.Count() ? -1 : 1;

			for(usys_t i = lhs.Count(); i > 0; i--)
			{
				if(lhs[i - 1] != rhs[i - 1])
					return lhs[i - 1] < rhs[i - 1] ? -1 : 1;
			}
			return 0;
		}

		template<typename TBuffer>
		void AddDigitsSmall(TBuffer& digits, const unsigned radix, unsigned value)
		{
			EL_ERROR(radix < 2 || radix > 256, error::TLogicException);
			for(usys_t i = 0; value != 0; i++)
			{
				if(i == digits.Count())
					digits.Append(0);

				const unsigned sum = (unsigned)digits[i] + value;
				digits[i] = (digit_t)(sum % radix);
				value = sum / radix;
			}
		}

		template<typename TBuffer>
		void AddDigits(TBuffer& lhs, const TBuffer& rhs, const unsigned radix)
		{
			const usys_t n = util::Max(lhs.Count(), rhs.Count());
			lhs.SetCount(n);
			unsigned carry = 0;
			for(usys_t i = 0; i < n; i++)
			{
				const unsigned sum = (unsigned)lhs[i] + (i < rhs.Count() ? (unsigned)rhs[i] : 0U) + carry;
				lhs[i] = (digit_t)(sum % radix);
				carry = sum / radix;
			}
			if(carry != 0)
				lhs.Append((digit_t)carry);
		}

		template<typename TBuffer>
		void SubtractDigits(TBuffer& lhs, const TBuffer& rhs, const unsigned radix)
		{
			EL_ERROR(CompareDigits(lhs, rhs) < 0, error::TLogicException);
			unsigned borrow = 0;
			for(usys_t i = 0; i < lhs.Count(); i++)
			{
				int value = (int)lhs[i] - (i < rhs.Count() ? (int)rhs[i] : 0) - (int)borrow;
				if(value < 0)
				{
					value += (int)radix;
					borrow = 1;
				}
				else
				{
					borrow = 0;
				}
				lhs[i] = (digit_t)value;
			}
			EL_ERROR(borrow != 0, error::TLogicException);
			TrimDigits(lhs);
		}

		template<typename TBuffer>
		void ShiftDigitsLeft(TBuffer& digits, const usys_t n_digits)
		{
			if(n_digits == 0 || IsDigitsZero(digits))
				return;

			const usys_t old_count = digits.Count();
			digits.SetCount(old_count + n_digits);
			for(usys_t i = old_count; i > 0; i--)
				digits[i - 1 + n_digits] = digits[i - 1];
			for(usys_t i = 0; i < n_digits; i++)
				digits[i] = 0;
		}

		template<typename TBuffer>
		void MultiplyDigitsSmall(TBuffer& digits, const unsigned radix, const unsigned value)
		{
			if(value == 0 || IsDigitsZero(digits))
			{
				digits.Truncate();
				return;
			}
			if(value == 1)
				return;
			if(value == radix)
			{
				ShiftDigitsLeft(digits, 1);
				return;
			}

			u64_t carry = 0;
			for(usys_t i = 0; i < digits.Count(); i++)
			{
				const u64_t product = (u64_t)digits[i] * value + carry;
				digits[i] = (digit_t)(product % radix);
				carry = product / radix;
			}
			while(carry != 0)
			{
				digits.Append((digit_t)(carry % radix));
				carry /= radix;
			}
		}

		template<typename TBuffer>
		void MultiplyDigitsPower(TBuffer& digits, const unsigned radix, const unsigned value, const usys_t exponent)
		{
			if(value == radix)
			{
				ShiftDigitsLeft(digits, exponent);
				return;
			}

			usys_t remaining = exponent;
			while(remaining != 0 && !IsDigitsZero(digits))
			{
				unsigned factor = 1;
				while(remaining != 0 && factor <= UINT_MAX / value)
				{
					factor *= value;
					remaining--;
				}
				MultiplyDigitsSmall(digits, radix, factor);
			}
		}

		template<typename TBuffer>
		void MultiplyDigits(TBuffer& result, const TBuffer& lhs, const TBuffer& rhs, const unsigned radix)
		{
			result.Truncate();
			if(IsDigitsZero(lhs) || IsDigitsZero(rhs))
				return;

			result.SetCount(lhs.Count() + rhs.Count());
			for(usys_t i = 0; i < lhs.Count(); i++)
			{
				unsigned carry = 0;
				for(usys_t j = 0; j < rhs.Count(); j++)
				{
					const usys_t k = i + j;
					const unsigned value = (unsigned)result[k] + (unsigned)lhs[i] * (unsigned)rhs[j] + carry;
					result[k] = (digit_t)(value % radix);
					carry = value / radix;
				}

				usys_t k = i + rhs.Count();
				while(carry != 0)
				{
					const unsigned value = (unsigned)result[k] + carry;
					result[k] = (digit_t)(value % radix);
					carry = value / radix;
					k++;
					if(k == result.Count() && carry != 0)
						result.Append(0);
				}
			}
			TrimDigits(result);
		}

		template<typename TBuffer>
		unsigned DivideDigitsSmall(TBuffer& digits, const unsigned radix, const unsigned divisor)
		{
			EL_ERROR(divisor == 0, error::TLogicException);
			u64_t remainder = 0;
			for(usys_t i = digits.Count(); i > 0; i--)
			{
				const u64_t value = remainder * radix + digits[i - 1];
				digits[i - 1] = (digit_t)(value / divisor);
				remainder = value % divisor;
			}
			TrimDigits(digits);
			return (unsigned)remainder;
		}

		template<typename TBuffer>
		void DivideDigitsPower(TBuffer& digits, const unsigned radix, const unsigned divisor, const usys_t exponent)
		{
			if(divisor == radix)
			{
				if(exponent >= digits.Count())
					digits.Truncate();
				else if(exponent != 0)
					digits.Cut(exponent, 0);
				return;
			}

			usys_t remaining = exponent;
			while(remaining != 0 && !IsDigitsZero(digits))
			{
				unsigned factor = 1;
				while(remaining != 0 && factor <= UINT_MAX / divisor)
				{
					factor *= divisor;
					remaining--;
				}
				DivideDigitsSmall(digits, radix, factor);
			}
		}

		template<typename TBuffer>
		void DivideDigits(TBuffer& quotient, TBuffer& remainder, const TBuffer& numerator, const TBuffer& denominator, const unsigned radix)
		{
			EL_ERROR(IsDigitsZero(denominator), error::TLogicException);
			quotient.Truncate();
			remainder.Truncate();
			if(IsDigitsZero(numerator))
				return;

			const int initial_cmp = CompareDigits(numerator, denominator);
			if(initial_cmp < 0)
			{
				remainder = numerator;
				return;
			}

			if(denominator.Count() == 1)
			{
				quotient = numerator;
				const unsigned rem = DivideDigitsSmall(quotient, radix, denominator[0]);
				if(rem != 0)
					remainder.Append((digit_t)rem);
				return;
			}

			TBuffer u = numerator;
			TBuffer v = denominator;
			const usys_t n = v.Count();
			const unsigned normalization = radix / ((unsigned)v[n - 1] + 1U);
			if(normalization > 1U)
			{
				MultiplyDigitsSmall(u, radix, normalization);
				MultiplyDigitsSmall(v, radix, normalization);
			}

			const usys_t numerator_count = numerator.Count();
			const usys_t m = numerator_count - denominator.Count();
			const usys_t required_u_count = numerator_count + 1U;
			if(u.Count() < required_u_count)
				u.SetCount(required_u_count);
			EL_ERROR(v.Count() != n, error::TLogicException);

			quotient.SetCount(m + 1U);
			for(usys_t jj = m + 1U; jj > 0; jj--)
			{
				const usys_t j = jj - 1U;
				const u64_t top = (u64_t)u[j + n] * radix + u[j + n - 1U];
				u64_t q_hat = top / v[n - 1U];
				u64_t r_hat = top % v[n - 1U];
				if(q_hat >= radix)
				{
					q_hat = radix - 1U;
					r_hat = top - q_hat * v[n - 1U];
				}

				while(q_hat * v[n - 2U] > r_hat * radix + u[j + n - 2U])
				{
					q_hat--;
					r_hat += v[n - 1U];
					if(r_hat >= radix)
						break;
				}

				u64_t borrow = 0;
				for(usys_t i = 0; i < n; i++)
				{
					const u64_t product = q_hat * v[i] + borrow;
					const unsigned low = (unsigned)(product % radix);
					borrow = product / radix;
					if(u[j + i] < low)
					{
						u[j + i] = (digit_t)((unsigned)u[j + i] + radix - low);
						borrow++;
					}
					else
					{
						u[j + i] = (digit_t)((unsigned)u[j + i] - low);
					}
				}

				const bool negative = (u64_t)u[j + n] < borrow;
				u[j + n] = (digit_t)((u64_t)u[j + n] + (negative ? radix : 0U) - borrow);
				if(negative)
				{
					q_hat--;
					unsigned carry = 0;
					for(usys_t i = 0; i < n; i++)
					{
						const unsigned sum = (unsigned)u[j + i] + v[i] + carry;
						u[j + i] = (digit_t)(sum % radix);
						carry = sum / radix;
					}
					u[j + n] = (digit_t)(((unsigned)u[j + n] + carry) % radix);
				}
				quotient[j] = (digit_t)q_hat;
			}
			TrimDigits(quotient);

			remainder.SetCount(n);
			for(usys_t i = 0; i < n; i++)
				remainder[i] = u[i];
			TrimDigits(remainder);
			if(normalization > 1U)
				DivideDigitsSmall(remainder, radix, normalization);
		}

		template<typename TBuffer>
		void ConvertDigitsRadix(TBuffer& result, const TBuffer& value, const unsigned source_radix, const unsigned target_radix)
		{
			if(source_radix == target_radix)
			{
				result = value;
				return;
			}

			result.Truncate();
			for(usys_t i = value.Count(); i > 0; i--)
			{
				MultiplyDigitsSmall(result, target_radix, source_radix);
				AddDigitsSmall(result, target_radix, value[i - 1]);
			}
		}

		template<typename TBuffer>
		unsigned ModuloDigitsSmall(const TBuffer& digits, const unsigned radix, const unsigned divisor) noexcept
		{
			if(divisor == 1U)
				return 0;
			u64_t remainder = 0;
			for(usys_t i = digits.Count(); i > 0; i--)
				remainder = (remainder * radix + digits[i - 1]) % divisor;
			return (unsigned)remainder;
		}

		inline unsigned GcdSmall(unsigned lhs, unsigned rhs) noexcept
		{
			while(rhs != 0)
			{
				const unsigned next = lhs % rhs;
				lhs = rhs;
				rhs = next;
			}
			return lhs;
		}

		// Every rational has an eventually periodic expansion. This returns true
		// only when the reduced denominator contains a factor that cannot be
		// consumed by powers of `output_radix`, i.e. the expansion cannot terminate.
		template<typename TBuffer>
		bool IsPeriodicFraction(const TBuffer& numerator, const TBuffer& denominator, const unsigned work_radix, const unsigned output_radix)
		{
			if(IsDigitsZero(numerator) || IsDigitsZero(denominator))
				return false;

			TBuffer a = numerator;
			TBuffer b = denominator;
			TBuffer quotient;
			TBuffer remainder;
			while(!IsDigitsZero(b))
			{
				DivideDigits(quotient, remainder, a, b, work_radix);
				a = b;
				b = remainder;
			}

			// denominator / gcd(numerator, denominator)
			DivideDigits(quotient, remainder, denominator, a, work_radix);
			EL_ERROR(!IsDigitsZero(remainder), error::TLogicException);
			while(!(quotient.Count() == 1U && quotient[0] == 1U))
			{
				const unsigned common = GcdSmall(output_radix, ModuloDigitsSmall(quotient, work_radix, output_radix));
				if(common == 1U)
					return true;
				EL_ERROR(DivideDigitsSmall(quotient, work_radix, common) != 0, error::TLogicException);
			}
			return false;
		}
	}

	/**
	 * Dynamically sized BCD value.
	 *
	 * Digits are owned by a TList<digit_t>. Integer and fractional precision are
	 * each limited to 65535 digits (`precision_t == u16_t`), which keeps the
	 * object compact while still allowing substantially larger values than any
	 * normal application should require.
	 * `n_integer` and `n_decimal` are deliberate per-value precision limits:
	 * arithmetic never grows them implicitly. This makes operations such as
	 * decimal 1/3 bounded by the output configuration instead of expanding
	 * until memory exhaustion. `SetPrecision()` changes those limits and can
	 * round discarded fractional digits explicitly.
	 */
	class TBCD
	{
		private:
			static io::collection::list::TList<digit_t> BuildMagnitudeDigits(const TBCD& value, unsigned target_radix);
			static bool AssignMagnitudeDigits(TBCD& out, const io::collection::list::TList<digit_t>& magnitude, bool is_negative);
			static int CompareMagnitude(const TBCD& lhs, const TBCD& rhs);
			static int AddGeneric(TBCD& out, const TBCD& lhs, const TBCD& rhs, bool subtract_rhs);
			static void MultiplyGeneric(TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static TBCD DivideGeneric(TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static io::collection::list::TList<digit_t> BuildIntegerDigits(u64_t value, unsigned radix);
			void ConvertFloatParts(const detail::TBinaryFloatParts& value);
			int CompareFloatingParts(const detail::TBinaryFloatParts& rhs) const;
			int CompareFloating(double rhs) const;

			template<usys_t, precision_t, precision_t, digit_t> friend class TFixedBCD;

		protected:
			// These functions perform unsigned operations.
			// They completely ignore `is_negative` and assume all numbers are positive.
			// The result (out) must not become negative (e.g. lhs must be >= rhs) for AbsSub().
			// The base of all arguments must match.
			static int  AbsAdd(TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static int  AbsSub(TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static void AbsMul(TBCD& out, const TBCD& lhs, const TBCD& rhs);

			io::collection::list::TList<digit_t> digits;
			precision_t n_integer;
			precision_t n_decimal;
			const digit_t base;	// 1 is invalid, 0 maps to 256
			u8_t is_negative : 1;
			mutable u8_t is_zero : 1;
			u8_t is_periodic : 1 = 0;
			u8_t value_class : 2 = (u8_t)EValueClass::FINITE;
			u8_t reserved_status_bits : 3 = 0;

			digit_t* DigitsPointer();
			const digit_t* DigitsPointer() const;

			// Allocates the configured digit storage in the TList and initializes it to 0.
			// Has no effect if the digits are already allocated.
			void EnsureDigits();

			template<typename T>
			void ConvertInteger(T value);
			void ConvertFloat(float value);
			void ConvertFloat(double value);
			void ConvertBCD(const TBCD& value);

		public:
			inline bool IsInvalid() const noexcept { return base == 1; }
			inline bool IsValid() const noexcept { return base != 1; }
			inline EValueClass ValueClass() const noexcept { return (EValueClass)value_class; }
			inline bool IsFinite() const noexcept { return IsValid() && ValueClass() == EValueClass::FINITE; }
			inline bool IsInfinity() const noexcept { return IsValid() && ValueClass() == EValueClass::INFINITE; }
			inline bool IsNaN() const noexcept { return IsValid() && ValueClass() == EValueClass::NOT_A_NUMBER; }

			// True when this value is a precision-limited result of a division whose exact
			// fractional expansion does not terminate in this base. This is metadata about
			// the exact division result; the stored digits themselves remain finite.
			inline bool IsPeriodic() const noexcept { return IsFinite() && is_periodic; }
			inline void IsPeriodic(const bool b) noexcept { is_periodic = b && IsFinite(); }

			// computes the number of required integer digits to fully represent the source value in the target base
			static usys_t RequiredDigits(const digit_t target_base, const digit_t source_base, const usys_t n_source_digits) EL_WARN_UNUSED_RESULT;

			// these function will return an empty array if the digits were not allocated yet!
			io::collection::array::array_t<const digit_t> Digits() const noexcept EL_GETTER;
			io::collection::array::array_t<const digit_t> IntegerDigits() const noexcept EL_GETTER;
			io::collection::array::array_t<const digit_t> DecimalDigits() const noexcept EL_GETTER;

			// Returns (or sets) the digit specified by `index`.
			// Positive values (>=0) access integer digits (little endian, lower index lower significance).
			// Negative values (<0) access decimal digits (again little endian).
			// If `index` is outside the bounds of `n_integer` and `n_decimal` respectively the getter function returns 0
			// The setter function will return false when the index was out of bounds, true otherwise.
			digit_t Digit(const ssys_t index) const EL_GETTER;
			bool Digit(const ssys_t index, const digit_t d) EL_SETTER;

			// Computes the index bounds for `this` and `rhs`, such that they cover all digits of both numbers.
			std::tuple<ssys_t,ssys_t> OuterBounds(const TBCD& rhs) const EL_GETTER;

			// Computes the index bounds for `this` and `rhs`, such that they only cover all digit places present in both numbers.
			std::tuple<ssys_t,ssys_t> InnerBounds(const TBCD& rhs) const EL_GETTER;

			digit_t Base() const EL_GETTER { return base; }
			unsigned Radix() const EL_GETTER { return base == 0 ? 256U : (unsigned)base; }
			usys_t CountDecimal() const EL_GETTER { return n_decimal; }
			usys_t CountInteger() const EL_GETTER { return n_integer; }
			bool IsNegative() const EL_GETTER { return is_negative && !IsNaN(); }
			void IsNegative(const bool b) EL_SETTER { if(!IsNaN()) is_negative = b; }

			// Returns the number of leading zeros on the integer part
			// (counted from most significant digit towards the least significant digit).
			// Essentially you could truncate the integer part by this amount of digits without altering the value.
			usys_t CountLeadingZeros() const EL_WARN_UNUSED_RESULT;

			// Returns the number of trailing zeros on the decimal part
			// (counted from least significant digit towards the most significant digit).
			// Essentially you could truncate the decimal part by this amount of digits without altering the value.
			usys_t CountTrailingZeros() const EL_WARN_UNUSED_RESULT;

			// The opposite of `CountLeadingZeros()`.
			// The amount of integer digits you need to keep to not alter the value.
			usys_t CountSignificantIntegerDigits() const EL_WARN_UNUSED_RESULT { return n_integer - CountLeadingZeros(); }

			// The opposite of `CountTrailingZeros()`.
			// The amount of decimal digits you need to keep to not alter the value.
			usys_t CountSignificantDecimalDigits() const EL_WARN_UNUSED_RESULT { return n_decimal - CountTrailingZeros(); }

			// Returns the index of the most significant non-zero digit.
			// Negative numbers address decimal places, >= 0 integer digits.
			// Throws an error if `IsZero()` is true.
			ssys_t IndexMostSignificantNonZeroDigit() const EL_WARN_UNUSED_RESULT;

			void Shift(const ssys_t s);

			TBCD& operator=(TBCD&& rhs);
			TBCD& operator=(const TBCD& rhs);
			TBCD& operator=(const double rhs);
			TBCD& operator=(const u64_t rhs);
			TBCD& operator=(const s64_t rhs);
			TBCD& operator=(const int rhs);

			// Below functions form the foundation of all operators of TBCD.
			// `Add()` and `Subtract()` return non-zero if significant integer digits were truncated.
			// `Multiply()` truncates to the output precision.
			// `Divide()` writes the truncated quotient to `out`. The returned residual uses the same configuration as `out`
			// and therefore depends on the quotient precision (e.g. 27.0 / 12.0 => 2.2 with residual 0.6).
			// All input and output argument may point to the same variable.
			// So in order to represent `a=a+a` you can write `Add(a,a,a)`.
			// The entire operation is performed with the configuration (base,
			// n_integer, n_decimal) of the output variable.
			// Inputs are copied and converted if the base does not match.
			// Naturally the value (`digits`, `is_negative`) of the output is overwritten.
			static int  Add      (TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static int  Subtract (TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static void Multiply (TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static TBCD Divide   (TBCD& out, const TBCD& lhs, const TBCD& rhs);

			TBCD& operator+=(const TBCD& rhs);
			TBCD& operator-=(const TBCD& rhs);
			TBCD& operator*=(const TBCD& rhs);
			TBCD& operator/=(const TBCD& rhs);
			TBCD& operator%=(const TBCD& rhs);
			TBCD& operator+=(const double rhs);
			TBCD& operator-=(const double rhs);
			TBCD& operator*=(const double rhs);
			TBCD& operator/=(const double rhs);
			TBCD& operator%=(const double rhs);
			TBCD& operator+=(const u64_t rhs);
			TBCD& operator-=(const u64_t rhs);
			TBCD& operator*=(const u64_t rhs);
			TBCD& operator/=(const u64_t rhs);
			TBCD& operator%=(const u64_t rhs);
			TBCD& operator+=(const s64_t rhs);
			TBCD& operator-=(const s64_t rhs);
			TBCD& operator*=(const s64_t rhs);
			TBCD& operator/=(const s64_t rhs);
			TBCD& operator%=(const s64_t rhs);
			TBCD& operator+=(const int rhs);
			TBCD& operator-=(const int rhs);
			TBCD& operator*=(const int rhs);
			TBCD& operator/=(const int rhs);
			TBCD& operator%=(const int rhs);
			TBCD& operator<<=(const unsigned n_shift);
			TBCD& operator>>=(const unsigned n_shift);

			TBCD operator+(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator-(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator*(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator/(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator%(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator+(const double rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator-(const double rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator*(const double rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator/(const double rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator%(const double rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator+(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator-(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator*(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator/(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator%(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator+(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator-(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator*(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator/(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator%(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator+(const int rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator-(const int rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator*(const int rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator/(const int rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator%(const int rhs) const EL_WARN_UNUSED_RESULT;
			TBCD operator<<(const unsigned n_shift) const EL_WARN_UNUSED_RESULT;
			TBCD operator>>(const unsigned n_shift) const EL_WARN_UNUSED_RESULT;

			bool operator==(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			bool operator!=(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			bool operator>=(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			bool operator<=(const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			bool operator> (const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			bool operator< (const TBCD& rhs) const EL_WARN_UNUSED_RESULT;
			bool operator==(const double rhs) const EL_GETTER;
			bool operator!=(const double rhs) const EL_GETTER;
			bool operator>=(const double rhs) const EL_GETTER;
			bool operator<=(const double rhs) const EL_GETTER;
			bool operator> (const double rhs) const EL_GETTER;
			bool operator< (const double rhs) const EL_GETTER;
			bool operator==(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator!=(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator>=(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator<=(const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator> (const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator< (const u64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator==(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator!=(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator>=(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator<=(const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator> (const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator< (const s64_t rhs) const EL_WARN_UNUSED_RESULT;
			bool operator==(const int rhs) const EL_WARN_UNUSED_RESULT;
			bool operator!=(const int rhs) const EL_WARN_UNUSED_RESULT;
			bool operator>=(const int rhs) const EL_WARN_UNUSED_RESULT;
			bool operator<=(const int rhs) const EL_WARN_UNUSED_RESULT;
			bool operator> (const int rhs) const EL_WARN_UNUSED_RESULT;
			bool operator< (const int rhs) const EL_WARN_UNUSED_RESULT;

			// This function does NOT change `n_decimal`, it just rounds the extra digits so they become 0.
			// If `n_decimal_max` >= `n_decimal` then this function has no effect.
			void Round(const usys_t n_decimal_max, math::ERoundingMode mode);

			// Changes the configured precision. Decimal digits discarded by shrinking are rounded first.
			// Returns true if non-zero integer digits had to be discarded. Growing precision preserves the value.
			bool SetPrecision(usys_t n_integer, usys_t n_decimal, math::ERoundingMode mode = math::ERoundingMode::TOWARDS_ZERO);

			// Sets a finite zero and clears periodic/special state.
			void SetZero() noexcept;
			void SetNaN() noexcept;
			void SetInfinity(bool negative = false) noexcept;

			// returns true if all digits are 0 (or were not allocated)
			bool IsZero() const noexcept EL_GETTER;

			double ToDouble() const EL_GETTER;

			// Removes the decimal digits and only returns the integer part.
			s64_t ToSignedInt() const EL_GETTER;
			u64_t ToUnsignedInt() const EL_GETTER;
			TBCD ToBCDInt() const EL_WARN_UNUSED_RESULT;

			explicit operator double() const EL_GETTER;
			explicit operator s64_t() const EL_GETTER;
			explicit operator u64_t() const EL_GETTER;

			// Returns true if `base`, `n_decimal` and `n_integer` are equal to `rhs`.
			bool HasSameSpecs(const TBCD& rhs) const EL_GETTER;

			// Returns a negative value if `this` < `rhs`.
			// Returns a positive value if `this` > `rhs`.
			// Returns zero if `this` == `rhs`.
			int Compare(const TBCD& rhs, const bool absolute = false) const EL_WARN_UNUSED_RESULT;

			TBCD(TBCD&& rhs) noexcept;
			TBCD(const TBCD&);

			// if `n_integer` and/or `n_decimal` is `AUTO_DETECT` then their value will be computed based on
			// the given base and the input datatype. For integer types `n_decimal` will be 0.
			// The base can be 0 in which case TBCD behaves like an IEEE integer with optional decimal digits and
			// adjustable size/precision. If `v` is 0 then no digits are allocated until a non-zero value is assigned.
			TBCD(const TBCD&  v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = AUTO_DETECT);
			TBCD(const u8_t   v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = 0);
			TBCD(const s8_t   v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = 0);
			TBCD(const u16_t  v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = 0);
			TBCD(const s16_t  v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = 0);
			TBCD(const u32_t  v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = 0);
			TBCD(const s32_t  v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = 0);
			TBCD(const u64_t  v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = 0);
			TBCD(const s64_t  v, const digit_t base, const usys_t n_integer = AUTO_DETECT, const usys_t n_decimal = 0);
			// For float input types the auto-detection does not work and `n_integer` and `n_decimal` must be manually set.
			// Floating-point values are decoded from their IEEE-754 bits and converted as the exact binary value.
			// Use FromString() when the decimal spelling (for example exact decimal 0.1) is intended instead.
			TBCD(const float  v, const digit_t base, const usys_t n_integer, const usys_t n_decimal);
			TBCD(const double v, const digit_t base, const usys_t n_integer, const usys_t n_decimal);

			// Creates a new TBCD with the same configuration as `conf_ref`.
			// If `v` is 0 then no digits are allocated until a non-zero value is assigned.
			TBCD(TBCD         v, const TBCD& conf_ref);
			TBCD(const u8_t   v, const TBCD& conf_ref);
			TBCD(const s8_t   v, const TBCD& conf_ref);
			TBCD(const u16_t  v, const TBCD& conf_ref);
			TBCD(const s16_t  v, const TBCD& conf_ref);
			TBCD(const u32_t  v, const TBCD& conf_ref);
			TBCD(const s32_t  v, const TBCD& conf_ref);
			TBCD(const u64_t  v, const TBCD& conf_ref);
			TBCD(const s64_t  v, const TBCD& conf_ref);
			TBCD(const float  v, const TBCD& conf_ref);
			TBCD(const double v, const TBCD& conf_ref);

			TBCD();
			~TBCD() = default;

			static const TBCD INVALID;

			// First digit in str is considered least significant value, while last position is most significant value (reversed to what humans do).
			static TBCD FromString(text::string::TStringView str, text::string::TStringView symbols, const char32_t decimal_seperator = '.', const char32_t negative_symbol = '-', const char32_t positive_symbol = '+', const bool default_negative = false);

			// Parses the conventional most-significant-digit-first spelling without reversing or copying the input.
			static TBCD FromStringMSD(text::string::TStringView str, text::string::TStringView symbols, const char32_t decimal_seperator = '.', const char32_t negative_symbol = '-', const char32_t positive_symbol = '+', const bool default_negative = false);
			// Fast path for conventional 2..36 digit alphabets (0-9, a-z/A-Z).
			static TBCD FromStringMSD(text::string::TStringView str, digit_t base, const char32_t decimal_seperator = '.', const char32_t negative_symbol = '-', const char32_t positive_symbol = '+', const bool default_negative = false);

			static TBCD Random(const digit_t base, const usys_t n_integer, const usys_t n_decimal);
	};


	/**
	 * Fixed-format BCD value.
	 *
	 * Capacity, integer digits, decimal digits and base are compile-time properties
	 * of the type. All digit storage is part of the object itself. Arithmetic between
	 * values of the same fixed type uses only fixed-size stack scratch buffers and
	 * never allocates. Conversion from a differently configured TFixedBCD may use
	 * the dynamic TBCD as conversion scratch storage.
	 */
	template<usys_t N_DIGITS, precision_t N_INTEGER, precision_t N_DECIMAL, digit_t BASE>
	class TFixedBCD
	{
		static_assert(N_DIGITS > 0, "TFixedBCD needs at least one digit of storage");
		static_assert(BASE != 1, "base 1 is reserved for invalid values");
		static_assert((usys_t)N_INTEGER + (usys_t)N_DECIMAL <= N_DIGITS, "configured precision exceeds fixed digit capacity");

		private:
			static constexpr precision_t n_integer = N_INTEGER;
			static constexpr precision_t n_decimal = N_DECIMAL;
			static constexpr digit_t base = BASE;
			digit_t digits[N_DIGITS] = {};
			u8_t is_negative : 1 = 0;
			u8_t is_periodic : 1 = 0;
			u8_t value_class : 2 = (u8_t)EValueClass::FINITE;
			u8_t reserved_status_bits : 4 = 0;


			template<typename T>
			void ConvertInteger(T value)
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

				SetZero();
				const unsigned radix = Radix();
				for(usys_t i = 0; i < n_integer && magnitude != 0; i++)
				{
					digits[n_decimal + i] = (digit_t)(magnitude % radix);
					magnitude /= radix;
				}
				is_negative = negative && !IsZero();
			}

			template<typename TFloat>
			requires std::is_floating_point_v<TFloat>
			void ConvertFloat(const TFloat value)
			{
				// Converting a differently represented value is allowed to use TBCD as
				// scratch storage. TBCD decodes the IEEE-754 bits directly and converts
				// the exact binary value to this compile-time radix/precision.
				AssignDynamic(TBCD(value, BASE, N_INTEGER, N_DECIMAL));
			}

			void AssignSameBase(const TBCD& value)
			{
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
				SetZero();
				for(ssys_t i = -(ssys_t)n_decimal; i < (ssys_t)n_integer; i++)
					Digit(i, value.Digit(i));
				is_negative = value.IsNegative() && !IsZero();
				is_periodic = value.IsPeriodic() && n_integer == value.CountInteger() && n_decimal == value.CountDecimal();
			}

			int CompareMagnitude(const TFixedBCD& rhs) const
			{
				const ssys_t low = -(ssys_t)util::Max(n_decimal, rhs.n_decimal);
				const ssys_t high = (ssys_t)util::Max(n_integer, rhs.n_integer) - 1;
				for(ssys_t i = high; i >= low; i--)
				{
					const digit_t l = Digit(i);
					const digit_t r = rhs.Digit(i);
					if(l != r)
						return l < r ? -1 : 1;
				}
				return 0;
			}

			static int AbsAdd(TFixedBCD& out, const TFixedBCD& lhs, const TFixedBCD& rhs)
			{
				const unsigned radix = out.Radix();
				const ssys_t low = -(ssys_t)util::Max(out.n_decimal, util::Max(lhs.n_decimal, rhs.n_decimal));
				const ssys_t high = (ssys_t)util::Max(out.n_integer, util::Max(lhs.n_integer, rhs.n_integer));
				const ssys_t out_low = -(ssys_t)out.n_decimal;
				const ssys_t out_high = (ssys_t)out.n_integer;
				unsigned carry = 0;
				bool overflow = false;

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
				return overflow || carry != 0 ? 1 : 0;
			}

			static int AbsSub(TFixedBCD& out, const TFixedBCD& lhs, const TFixedBCD& rhs)
			{
				EL_ERROR(lhs.CompareMagnitude(rhs) < 0, error::TLogicException);
				const unsigned radix = out.Radix();
				const ssys_t low = -(ssys_t)util::Max(out.n_decimal, util::Max(lhs.n_decimal, rhs.n_decimal));
				const ssys_t high = (ssys_t)util::Max(out.n_integer, util::Max(lhs.n_integer, rhs.n_integer));
				const ssys_t out_low = -(ssys_t)out.n_decimal;
				const ssys_t out_high = (ssys_t)out.n_integer;
				unsigned borrow = 0;
				bool overflow = false;

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
				EL_ERROR(borrow != 0, error::TLogicException);
				return overflow ? 1 : 0;
			}

			template<usys_t WORK_DIGITS>
			void LoadMagnitude(detail::TFixedList<digit_t, WORK_DIGITS>& out) const
			{
				out.Truncate();
				if(IsZero())
					return;
				const usys_t n = n_integer + n_decimal;
				out.SetCount(n);
				for(usys_t i = 0; i < n; i++)
					out[i] = digits[i];
				detail::TrimDigits(out);
			}

			template<usys_t WORK_DIGITS>
			bool AssignMagnitude(const detail::TFixedList<digit_t, WORK_DIGITS>& magnitude, const bool negative)
			{
				SetZero();
				const usys_t n = n_integer + n_decimal;
				const usys_t n_copy = util::Min(n, magnitude.Count());
				for(usys_t i = 0; i < n_copy; i++)
					digits[i] = magnitude[i];
				is_negative = negative && !IsZero();
				return magnitude.Count() > n;
			}

			void AssignDynamic(const TBCD& value)
			{
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
				if(value.Base() == base)
				{
					AssignSameBase(value);
					return;
				}
				const TBCD converted(value, base, n_integer, n_decimal);
				AssignSameBase(converted);
			}

			template<usys_t OTHER_N_DIGITS, precision_t OTHER_N_INTEGER, precision_t OTHER_N_DECIMAL, digit_t OTHER_BASE>
			void AssignFixed(const TFixedBCD<OTHER_N_DIGITS, OTHER_N_INTEGER, OTHER_N_DECIMAL, OTHER_BASE>& value)
			{
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

				if constexpr(OTHER_BASE == BASE)
				{
					SetZero();
					for(ssys_t i = -(ssys_t)n_decimal; i < (ssys_t)n_integer; i++)
						Digit(i, value.Digit(i));
					is_negative = value.IsNegative() && !IsZero();
					is_periodic = value.IsPeriodic() && N_INTEGER == OTHER_N_INTEGER && N_DECIMAL == OTHER_N_DECIMAL;
					return;
				}

				AssignDynamic(value.ToBCD());
			}

			template<typename T>
			requires std::is_integral_v<T>
			int CompareIntegral(const T rhs) const noexcept
			{
				if(IsNaN())
					return 2;
				if(IsInfinity())
					return IsNegative() ? -1 : 1;
				using unsigned_t = std::make_unsigned_t<T>;
				const bool rhs_negative = std::is_signed_v<T> && rhs < 0;
				const unsigned_t rhs_bits = (unsigned_t)rhs;
				unsigned_t magnitude = rhs_negative ? (unsigned_t)((unsigned_t)0 - rhs_bits) : rhs_bits;
				const bool lhs_negative = is_negative && !IsZero();

				if(lhs_negative != rhs_negative)
					return lhs_negative ? -1 : 1;

				digit_t rhs_digits[sizeof(T) * CHAR_BIT] = {};
				usys_t n_rhs_digits = 0;
				const unsigned radix = Radix();
				while(magnitude != 0)
				{
					rhs_digits[n_rhs_digits++] = (digit_t)(magnitude % radix);
					magnitude /= radix;
				}

				const ssys_t high = (ssys_t)util::Max<usys_t>(n_integer, n_rhs_digits) - 1;
				for(ssys_t i = high; i >= -(ssys_t)n_decimal; i--)
				{
					const digit_t l = Digit(i);
					const digit_t r = i >= 0 && (usys_t)i < n_rhs_digits ? rhs_digits[i] : 0;
					if(l != r)
					{
						const int cmp = l < r ? -1 : 1;
						return lhs_negative ? -cmp : cmp;
					}
				}
				return 0;
			}

			int CompareFloating(const double rhs) const
			{
				return ToBCD().CompareFloating(rhs);
			}

		public:
			static constexpr usys_t CAPACITY = N_DIGITS;
			static constexpr precision_t INTEGER_DIGITS = N_INTEGER;
			static constexpr precision_t DECIMAL_DIGITS = N_DECIMAL;
			static constexpr digit_t NUMERIC_BASE = BASE;

			TFixedBCD() = default;

			template<typename T>
			requires std::is_integral_v<T>
			TFixedBCD(const T value)
			{
				ConvertInteger(value);
			}

			TFixedBCD(const double value)
			{
				ConvertFloat(value);
			}

			TFixedBCD(const float value)
			{
				ConvertFloat(value);
			}

			TFixedBCD(const TBCD& value)
			{
				AssignDynamic(value);
			}

			template<usys_t OTHER_N_DIGITS, precision_t OTHER_N_INTEGER, precision_t OTHER_N_DECIMAL, digit_t OTHER_BASE>
			TFixedBCD(const TFixedBCD<OTHER_N_DIGITS, OTHER_N_INTEGER, OTHER_N_DECIMAL, OTHER_BASE>& value)
			{
				AssignFixed(value);
			}

			bool IsInvalid() const noexcept { return base == 1; }
			bool IsValid() const noexcept { return base != 1; }
			EValueClass ValueClass() const noexcept { return (EValueClass)value_class; }
			bool IsFinite() const noexcept { return IsValid() && ValueClass() == EValueClass::FINITE; }
			bool IsInfinity() const noexcept { return IsValid() && ValueClass() == EValueClass::INFINITE; }
			bool IsNaN() const noexcept { return IsValid() && ValueClass() == EValueClass::NOT_A_NUMBER; }

			// See TBCD::IsPeriodic(). The flag never changes the numeric value of the
			// stored finite digits and is cleared by operations that invalidate its origin.
			bool IsPeriodic() const noexcept { return IsFinite() && is_periodic; }
			void IsPeriodic(const bool value) noexcept { is_periodic = value && IsFinite(); }
			static constexpr digit_t Base() noexcept { return base; }
			static constexpr unsigned Radix() noexcept { return base == 0 ? 256U : (unsigned)base; }
			static constexpr usys_t CountInteger() noexcept { return n_integer; }
			static constexpr usys_t CountDecimal() noexcept { return n_decimal; }
			bool IsNegative() const noexcept { return is_negative && !IsNaN(); }
			void IsNegative(const bool value) noexcept { if(!IsNaN()) is_negative = value; }

			io::collection::array::array_t<const digit_t> Digits() const noexcept
			{
				return !IsFinite() ? io::collection::array::array_t<const digit_t>() : io::collection::array::array_t<const digit_t>::FromUnsafePointer(digits, n_integer + n_decimal);
			}

			io::collection::array::array_t<const digit_t> IntegerDigits() const noexcept
			{
				return !IsFinite() ? io::collection::array::array_t<const digit_t>() : io::collection::array::array_t<const digit_t>::FromUnsafePointer(digits + n_decimal, n_integer);
			}

			io::collection::array::array_t<const digit_t> DecimalDigits() const noexcept
			{
				return !IsFinite() ? io::collection::array::array_t<const digit_t>() : io::collection::array::array_t<const digit_t>::FromUnsafePointer(digits, n_decimal);
			}

			digit_t Digit(const ssys_t index) const noexcept
			{
				const ssys_t storage_index = index + (ssys_t)n_decimal;
				return storage_index < 0 || (usys_t)storage_index >= n_integer + n_decimal ? 0 : digits[storage_index];
			}

			bool Digit(const ssys_t index, const digit_t value)
			{
				EL_ERROR(!IsFinite(), error::TInvalidArgumentException, "this", "special BCD value has no digits");
				const ssys_t storage_index = index + (ssys_t)n_decimal;
				if(storage_index < 0 || (usys_t)storage_index >= n_integer + n_decimal)
					return false;
				EL_ERROR((unsigned)value >= Radix(), error::TInvalidArgumentException, "value", "digit must be smaller than the numeric base");
				digits[storage_index] = value;
				is_periodic = false;
				return true;
			}

			void SetZero() noexcept
			{
				for(usys_t i = 0; i < n_integer + n_decimal; i++)
					digits[i] = 0;
				is_negative = false;
				is_periodic = false;
				value_class = (u8_t)EValueClass::FINITE;
			}

			void SetNaN() noexcept
			{
				for(usys_t i = 0; i < n_integer + n_decimal; i++)
					digits[i] = 0;
				is_negative = false;
				is_periodic = false;
				value_class = (u8_t)EValueClass::NOT_A_NUMBER;
			}

			void SetInfinity(const bool negative = false) noexcept
			{
				for(usys_t i = 0; i < n_integer + n_decimal; i++)
					digits[i] = 0;
				is_negative = negative;
				is_periodic = false;
				value_class = (u8_t)EValueClass::INFINITE;
			}

			bool IsZero() const noexcept
			{
				if(!IsFinite())
					return false;
				for(usys_t i = 0; i < n_integer + n_decimal; i++)
					if(digits[i] != 0)
						return false;
				return true;
			}

			usys_t CountLeadingZeros() const noexcept
			{
				if(!IsFinite())
					return 0;
				usys_t count = 0;
				for(usys_t i = n_integer; i > 0 && Digit((ssys_t)i - 1) == 0; i--)
					count++;
				return count;
			}

			usys_t CountTrailingZeros() const noexcept
			{
				if(!IsFinite())
					return 0;
				usys_t count = 0;
				for(usys_t i = n_decimal; i > 0 && Digit(-(ssys_t)i) == 0; i--)
					count++;
				return count;
			}

			usys_t CountSignificantIntegerDigits() const noexcept { return n_integer - CountLeadingZeros(); }
			usys_t CountSignificantDecimalDigits() const noexcept { return n_decimal - CountTrailingZeros(); }

			void Round(const usys_t n_decimal_max, const math::ERoundingMode mode)
			{
				if(!IsFinite())
					return;
				if(n_decimal_max >= n_decimal || IsZero())
					return;
				is_periodic = false;

				const usys_t n_discard = n_decimal - n_decimal_max;
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
				const digit_t first_discarded = digits[n_discard - 1];
				bool increment = false;
				switch(mode)
				{
					case math::ERoundingMode::TOWARDS_ZERO:
						break;
					case math::ERoundingMode::AWAY_FROM_ZERO:
						increment = true;
						break;
					case math::ERoundingMode::DOWNWARD:
						increment = is_negative;
						break;
					case math::ERoundingMode::UPWARD:
						increment = !is_negative;
						break;
					case math::ERoundingMode::TO_NEAREST:
					case math::ERoundingMode::TO_NEAREST_EVEN:
					{
						const unsigned twice = (unsigned)first_discarded * 2U;
						bool greater_than_half = twice > radix;
						bool exactly_half = twice == radix;
						if(exactly_half)
							for(usys_t i = 0; i + 1 < n_discard; i++)
								if(digits[i] != 0)
								{
									greater_than_half = true;
									exactly_half = false;
									break;
								}
						if(greater_than_half)
							increment = true;
						else if(exactly_half)
							increment = mode == math::ERoundingMode::TO_NEAREST || ((Digit((ssys_t)n_discard - (ssys_t)n_decimal) & 1U) != 0);
						break;
					}
					case math::ERoundingMode::STOCHASTIC:
					{
						auto& rng = system::random::TSystemRandom::Instance();
						for(usys_t i = n_discard; i > 0; i--)
						{
							const unsigned random_digit = rng.IntegerRange<unsigned>(0, radix);
							const unsigned discarded_digit = digits[i - 1];
							if(random_digit < discarded_digit)
							{
								increment = true;
								break;
							}
							if(random_digit > discarded_digit)
								break;
						}
						break;
					}
				}

				for(usys_t i = 0; i < n_discard; i++)
					digits[i] = 0;
				if(increment)
				{
					unsigned carry = 1;
					for(usys_t i = n_discard; carry != 0 && i < n_integer + n_decimal; i++)
					{
						const unsigned value = (unsigned)digits[i] + carry;
						digits[i] = (digit_t)(value % radix);
						carry = value / radix;
					}
				}
			}


			TBCD ToBCD() const
			{
				TBCD out(0, base, n_integer, n_decimal);
				if(IsNaN())
				{
					out.SetNaN();
					return out;
				}
				if(IsInfinity())
				{
					out.SetInfinity(IsNegative());
					return out;
				}
				for(ssys_t i = -(ssys_t)n_decimal; i < (ssys_t)n_integer; i++)
					out.Digit(i, Digit(i));
				out.IsNegative(is_negative);
				out.IsPeriodic(is_periodic);
				return out;
			}

			TFixedBCD& operator=(const TBCD& rhs)
			{
				AssignDynamic(rhs);
				return *this;
			}

			template<usys_t OTHER_N_DIGITS, precision_t OTHER_N_INTEGER, precision_t OTHER_N_DECIMAL, digit_t OTHER_BASE>
			TFixedBCD& operator=(const TFixedBCD<OTHER_N_DIGITS, OTHER_N_INTEGER, OTHER_N_DECIMAL, OTHER_BASE>& rhs)
			{
				AssignFixed(rhs);
				return *this;
			}

			template<typename T>
			requires std::is_integral_v<T>
			TFixedBCD& operator=(const T rhs)
			{
				ConvertInteger(rhs);
				return *this;
			}

			TFixedBCD& operator=(const double rhs)
			{
				ConvertFloat(rhs);
				return *this;
			}

			static int Add(TFixedBCD& out, const TFixedBCD& lhs, const TFixedBCD& rhs)
			{
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
				out.is_periodic = false;

				const bool lhs_negative = lhs.is_negative && !lhs.IsZero();
				const bool rhs_negative = rhs.is_negative && !rhs.IsZero();
				if(lhs_negative == rhs_negative)
				{
					const int overflow = AbsAdd(out, lhs, rhs);
					out.is_negative = lhs_negative && !out.IsZero();
					return overflow;
				}

				const int cmp = lhs.CompareMagnitude(rhs);
				if(cmp == 0)
				{
					out.SetZero();
					return 0;
				}
				if(cmp > 0)
				{
					const int overflow = AbsSub(out, lhs, rhs);
					out.is_negative = lhs_negative && !out.IsZero();
					return overflow;
				}
				const int overflow = AbsSub(out, rhs, lhs);
				out.is_negative = rhs_negative && !out.IsZero();
				return overflow;
			}

			static int Subtract(TFixedBCD& out, const TFixedBCD& lhs, const TFixedBCD& rhs)
			{
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
				TFixedBCD neg_rhs(rhs);
				neg_rhs.is_negative = !neg_rhs.is_negative;
				return Add(out, lhs, neg_rhs);
			}

			static void Multiply(TFixedBCD& out, const TFixedBCD& lhs, const TFixedBCD& rhs)
			{
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
				out.is_periodic = false;

				using TWork = detail::TFixedList<digit_t, N_DIGITS * 3U + 4U>;
				TWork left;
				TWork right;
				TWork product;
				lhs.LoadMagnitude(left);
				rhs.LoadMagnitude(right);
				detail::MultiplyDigits(product, left, right, out.Radix());
				const usys_t source_decimal = lhs.n_decimal + rhs.n_decimal;
				if(out.n_decimal >= source_decimal)
					detail::ShiftDigitsLeft(product, out.n_decimal - source_decimal);
				else
					detail::DivideDigitsPower(product, out.Radix(), out.Radix(), source_decimal - out.n_decimal);
				out.AssignMagnitude(product, (lhs.is_negative != rhs.is_negative) && !detail::IsDigitsZero(product));
			}

			static TFixedBCD Divide(TFixedBCD& out, const TFixedBCD& lhs, const TFixedBCD& rhs)
			{
				if(lhs.IsNaN() || rhs.IsNaN())
				{
					out.SetNaN();
					TFixedBCD remainder(0);
					remainder.SetNaN();
					return remainder;
				}
				if(lhs.IsInfinity() && rhs.IsInfinity())
				{
					out.SetNaN();
					TFixedBCD remainder(0);
					remainder.SetNaN();
					return remainder;
				}
				if(rhs.IsInfinity())
				{
					TFixedBCD remainder(0);
					TFixedBCD zero(0);
					Add(remainder, lhs, zero);
					out.SetZero();
					return remainder;
				}
				if(lhs.IsInfinity())
				{
					out.SetInfinity(lhs.IsNegative() != rhs.IsNegative());
					TFixedBCD remainder(0);
					remainder.SetNaN();
					return remainder;
				}
				if(rhs.IsZero())
				{
					TFixedBCD remainder(0);
					if(lhs.IsZero())
						out.SetNaN();
					else
						out.SetInfinity(lhs.IsNegative() != rhs.IsNegative());
					remainder.SetNaN();
					return remainder;
				}
				if(!out.IsFinite())
					out.SetZero();
				out.is_periodic = false;

				using TWork = detail::TFixedList<digit_t, N_DIGITS * 3U + 4U>;
				TWork numerator;
				TWork denominator;
				TWork quotient;
				TWork remainder_digits;
				lhs.LoadMagnitude(numerator);
				rhs.LoadMagnitude(denominator);
				EL_ERROR(detail::IsDigitsZero(denominator), error::TInvalidArgumentException, "rhs", "divisor cannot be zero");
				detail::ShiftDigitsLeft(numerator, rhs.n_decimal + out.n_decimal);
				detail::ShiftDigitsLeft(denominator, lhs.n_decimal);
				detail::DivideDigits(quotient, remainder_digits, numerator, denominator, out.Radix());
				const bool periodic = !detail::IsDigitsZero(remainder_digits) && detail::IsPeriodicFraction(numerator, denominator, out.Radix(), out.Radix());
				detail::DivideDigitsPower(remainder_digits, out.Radix(), out.Radix(), lhs.n_decimal + rhs.n_decimal);

				const bool quotient_negative = (lhs.is_negative != rhs.is_negative) && !detail::IsDigitsZero(quotient);
				const bool remainder_negative = lhs.is_negative && !detail::IsDigitsZero(remainder_digits);
				out.AssignMagnitude(quotient, quotient_negative);
				out.is_periodic = periodic;
				TFixedBCD remainder(0);
				remainder.AssignMagnitude(remainder_digits, remainder_negative);
				return remainder;
			}

			int Compare(const TFixedBCD& rhs, const bool absolute = false) const
			{
				EL_ERROR(IsNaN() || rhs.IsNaN(), error::TInvalidArgumentException, "rhs", "comparison with NaN is unordered");
				if(IsInfinity() || rhs.IsInfinity())
				{
					if(IsInfinity() && rhs.IsInfinity())
					{
						if(absolute || IsNegative() == rhs.IsNegative())
							return 0;
						return IsNegative() ? -1 : 1;
					}
					if(absolute)
						return IsInfinity() ? 1 : -1;
					if(IsInfinity())
						return IsNegative() ? -1 : 1;
					return rhs.IsNegative() ? 1 : -1;
				}
				if(!absolute)
				{
					const bool lhs_negative = is_negative && !IsZero();
					const bool rhs_negative = rhs.is_negative && !rhs.IsZero();
					if(lhs_negative != rhs_negative)
						return lhs_negative ? -1 : 1;
					const int cmp = CompareMagnitude(rhs);
					return lhs_negative ? -cmp : cmp;
				}
				return CompareMagnitude(rhs);
			}

			double ToDouble() const noexcept
			{
				if(IsNaN())
					return std::numeric_limits<double>::quiet_NaN();
				if(IsInfinity())
					return IsNegative() ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
				long double value = 0;
				const unsigned radix = Radix();
				for(usys_t i = n_integer + n_decimal; i > 0; i--)
					value = value * radix + digits[i - 1];
				for(usys_t i = 0; i < n_decimal; i++)
					value /= radix;
				return (double)(is_negative ? -value : value);
			}

			s64_t ToSignedInt() const
			{
				EL_ERROR(!IsFinite(), error::TInvalidArgumentException, "this", "special BCD value cannot be converted to an integer");
				u64_t magnitude = ToUnsignedInt();
				if(is_negative && magnitude != 0)
					magnitude = 0U - magnitude;
				s64_t value;
				std::memcpy(&value, &magnitude, sizeof(value));
				return value;
			}

			u64_t ToUnsignedInt() const
			{
				EL_ERROR(!IsFinite(), error::TInvalidArgumentException, "this", "special BCD value cannot be converted to an integer");
				u64_t value = 0;
				const unsigned radix = Radix();
				for(usys_t i = n_integer; i > 0; i--)
					value = value * radix + Digit((ssys_t)i - 1);
				return value;
			}

			template<typename T>
			requires std::is_integral_v<T>
			bool TryToInteger(T& out) const noexcept
			{
				if(!IsFinite())
					return false;
				for(usys_t i = 0; i < n_decimal; i++)
					if(digits[i] != 0)
						return false;

				if constexpr(std::same_as<std::remove_cv_t<T>, bool>)
				{
					if(IsNegative())
						return false;
					unsigned magnitude = 0;
					for(usys_t i = n_integer; i > 0; i--)
					{
						const unsigned digit = (unsigned)Digit((ssys_t)i - 1);
						if(digit > 1U || magnitude != 0U)
							return false;
						magnitude = digit;
					}
					out = magnitude != 0U;
					return true;
				}
				else
				{
					using unsigned_t = std::make_unsigned_t<T>;
					if constexpr(!std::is_signed_v<T>)
						if(IsNegative())
							return false;

					const bool negative = std::is_signed_v<T> && IsNegative();
					const unsigned_t limit = [&]() -> unsigned_t
					{
						if constexpr(std::is_signed_v<T>)
							return negative
								? (unsigned_t)0 - (unsigned_t)std::numeric_limits<T>::min()
								: (unsigned_t)std::numeric_limits<T>::max();
						else
							return std::numeric_limits<T>::max();
					}();

					unsigned_t magnitude = 0;
					const unsigned_t radix = (unsigned_t)Radix();
					for(usys_t i = n_integer; i > 0; i--)
					{
						const unsigned_t digit = (unsigned_t)Digit((ssys_t)i - 1);
						if(digit > limit || magnitude > (limit - digit) / radix)
							return false;
						magnitude = magnitude * radix + digit;
					}

					if constexpr(std::is_signed_v<T>)
					{
						if(negative)
							out = magnitude == limit ? std::numeric_limits<T>::min() : (T)-(T)magnitude;
						else
							out = (T)magnitude;
					}
					else
					{
						out = (T)magnitude;
					}
					return true;
				}
			}

			TFixedBCD& operator+=(const TFixedBCD& rhs) { Add(*this, *this, rhs); return *this; }
			TFixedBCD& operator-=(const TFixedBCD& rhs) { Subtract(*this, *this, rhs); return *this; }
			TFixedBCD& operator*=(const TFixedBCD& rhs) { Multiply(*this, *this, rhs); return *this; }
			TFixedBCD& operator/=(const TFixedBCD& rhs) { Divide(*this, *this, rhs); return *this; }
			TFixedBCD& operator%=(const TFixedBCD& rhs)
			{
				TFixedBCD quotient(0);
				*this = Divide(quotient, *this, rhs);
				return *this;
			}

			TFixedBCD operator+(const TFixedBCD& rhs) const { TFixedBCD out(*this); out += rhs; return out; }
			TFixedBCD operator-(const TFixedBCD& rhs) const { TFixedBCD out(*this); out -= rhs; return out; }
			TFixedBCD operator*(const TFixedBCD& rhs) const { TFixedBCD out(*this); out *= rhs; return out; }
			TFixedBCD operator/(const TFixedBCD& rhs) const { TFixedBCD out(*this); out /= rhs; return out; }
			TFixedBCD operator%(const TFixedBCD& rhs) const { TFixedBCD out(*this); out %= rhs; return out; }

			bool operator==(const TFixedBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) == 0; }
			bool operator!=(const TFixedBCD& rhs) const { return IsNaN() || rhs.IsNaN() || Compare(rhs) != 0; }
			bool operator<(const TFixedBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) < 0; }
			bool operator<=(const TFixedBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) <= 0; }
			bool operator>(const TFixedBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) > 0; }
			bool operator>=(const TFixedBCD& rhs) const { return !IsNaN() && !rhs.IsNaN() && Compare(rhs) >= 0; }

			template<typename T>
			requires std::is_integral_v<T>
			bool operator==(const T rhs) const noexcept { return !IsNaN() && CompareIntegral(rhs) == 0; }
			template<typename T>
			requires std::is_integral_v<T>
			bool operator!=(const T rhs) const noexcept { return IsNaN() || CompareIntegral(rhs) != 0; }
			template<typename T>
			requires std::is_integral_v<T>
			bool operator<(const T rhs) const noexcept { return !IsNaN() && CompareIntegral(rhs) < 0; }
			template<typename T>
			requires std::is_integral_v<T>
			bool operator<=(const T rhs) const noexcept { return !IsNaN() && CompareIntegral(rhs) <= 0; }
			template<typename T>
			requires std::is_integral_v<T>
			bool operator>(const T rhs) const noexcept { return !IsNaN() && CompareIntegral(rhs) > 0; }
			template<typename T>
			requires std::is_integral_v<T>
			bool operator>=(const T rhs) const noexcept { return !IsNaN() && CompareIntegral(rhs) >= 0; }

			bool operator==(const double rhs) const { return CompareFloating(rhs) == 0; }
			bool operator!=(const double rhs) const { return CompareFloating(rhs) != 0; }
			bool operator<(const double rhs) const { return CompareFloating(rhs) == -1; }
			bool operator<=(const double rhs) const { const int c = CompareFloating(rhs); return c != 2 && c <= 0; }
			bool operator>(const double rhs) const { return CompareFloating(rhs) == 1; }
			bool operator>=(const double rhs) const { const int c = CompareFloating(rhs); return c != 2 && c >= 0; }

			explicit operator TBCD() const { return ToBCD(); }
			explicit operator double() const noexcept { return ToDouble(); }
	};
}
