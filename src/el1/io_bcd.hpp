#pragma once

#include "error.hpp"
#include "math.hpp"
#include "io_types.hpp"
#include "io_collection_list.hpp"
#include "io_text_encoding.hpp"
#include <memory>

namespace el1::io::text::string
{
	class TString;
}

namespace el1::io::bcd
{
	using namespace types;
	using digit_t = u8_t;
	static const u8_t AUTO_DETECT = 255;

	class alignas(digit_t*) TBCD
	{
		protected:
			// These functions perform unsigned operations.
			// They completely ignore `is_negative` and assume all numbers are positive.
			// The result (out) must not become negative (e.g. lhs must be >= rhs) for AbsSub().
			// The base of all arguments must match.
			static int  AbsAdd(TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static int  AbsSub(TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static void AbsMul(TBCD& out, const TBCD& lhs, const TBCD& rhs);
			static TBCD AbsDiv(TBCD& out, const TBCD& lhs, const TBCD& rhs);

			const digit_t base;	// 1 is invalid, 0 maps to 256
			const u8_t n_integer;
			const u8_t n_decimal;
			u8_t is_negative : 1;
			mutable u8_t is_zero : 1;

			using uptr_t = std::unique_ptr<digit_t[]>;
			byte_t _mem[sizeof(void*) == 8 ? 12 : sizeof(void*)];
			inline bool InternalMemory() const { return (n_decimal + n_integer) * sizeof(digit_t) <= sizeof(_mem); }

			void InitMem();
			digit_t* DigitsPointer();
			const digit_t* DigitsPointer() const;

			// Allocates memory for the digits and memset()'s them to 0.
			// Has no effect if the digits are already allocated.
			void EnsureDigits();

			template<typename T>
			void ConvertInteger(T value);
			void ConvertFloat(double value);
			void ConvertBCD(const TBCD& value);

		public:
			inline bool IsInvalid() const { return base == 1; }
			inline bool IsValid() const { return base != 1; }

			// computes the number of required integer digits to fully represent the source value in the target base
			static u8_t RequiredDigits(const digit_t target_base, const digit_t source_base, const unsigned n_source_digits) EL_GETTER;

			// these function will return an empty array if the digits were not allocated yet!
			io::collection::list::array_t<const digit_t> Digits() const noexcept EL_GETTER;
			io::collection::list::array_t<const digit_t> IntegerDigits() const noexcept EL_GETTER;
			io::collection::list::array_t<const digit_t> DecimalDigits() const noexcept EL_GETTER;

			// Returns (or sets) the digit specified by `index`.
			// Positive values (>=0) access integer digits (little endian, lower index lower significance).
			// Negative values (<0) access decimal digits (again little endian).
			// If `index` is outside the bounds of `n_integer` and `n_decimal` respectively the getter function returns 0
			// The setter function will return false when the index was out of bounds, true otherwise.
			digit_t Digit(const int index) const EL_GETTER;
			bool Digit(const int index, const digit_t d) EL_SETTER;

			// Computes the index bounds for `this` and `rhs`, such that they cover all digits of both numbers.
			std::tuple<int,int> OuterBounds(const TBCD& rhs) const EL_GETTER;

			// Computes the index bounds for `this` and `rhs`, such that they only cover all digit places present in both numbers.
			std::tuple<int,int> InnerBounds(const TBCD& rhs) const EL_GETTER;

			digit_t Base() const EL_GETTER { return base; }
			u8_t CountDecimal() const EL_GETTER { return n_decimal; }
			u8_t CountInteger() const EL_GETTER { return n_integer; }
			bool IsNegative() const EL_GETTER { return is_negative; }
			void IsNegative(const bool b) EL_SETTER { is_negative = b; }

			// Returns the number of leading zeros on the integer part
			// (counted from most significant digit towards the least significant digit).
			// Essentially you could truncate the integer part by this amount of digits without altering the value.
			u8_t CountLeadingZeros() const EL_GETTER;

			// Returns the number of trailing zeros on the decimal part
			// (counted from least significant digit towards the most significant digit).
			// Essentially you could truncate the decimal part by this amount of digits without altering the value.
			u8_t CountTrailingZeros() const EL_GETTER;

			// The opposite of `CountLeadingZeros()`.
			// The amount of integer digits you need to keep to not alter the value.
			u8_t CountSignificantIntegerDigits() const EL_GETTER { return n_integer - CountLeadingZeros(); }

			// The opposite of `CountTrailingZeros()`.
			// The amount of decimal digits you need to keep to not alter the value.
			u8_t CountSignificantDecimalDigits() const EL_GETTER { return n_decimal - CountTrailingZeros(); }

			// Returns the index of the most significant non-zero digit.
			// Negative numbers address decimal places, >= 0 integer digits.
			// Throws an error if `IsZero()` is true.
			int IndexMostSignificantNonZeroDigit() const EL_GETTER;

			void Shift(const int s);

			TBCD& operator=(TBCD&& rhs);
			TBCD& operator=(const TBCD& rhs);
			TBCD& operator=(const double rhs);
			TBCD& operator=(const u64_t rhs);
			TBCD& operator=(const s64_t rhs);
			TBCD& operator=(const int rhs);

			// Below functions form the foundation of all operators of TBCD.
			// `Add()`, `Subtract()` return the carry (if any).
			// `Multiply()` returns whether or not there was a numeric overflow on the output.
			// `Divide()` returns whether or not there was a remainder.
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

			TBCD operator+(const TBCD& rhs) const EL_GETTER;
			TBCD operator-(const TBCD& rhs) const EL_GETTER;
			TBCD operator*(const TBCD& rhs) const EL_GETTER;
			TBCD operator/(const TBCD& rhs) const EL_GETTER;
			TBCD operator%(const TBCD& rhs) const EL_GETTER;
			TBCD operator+(const double rhs) const EL_GETTER;
			TBCD operator-(const double rhs) const EL_GETTER;
			TBCD operator*(const double rhs) const EL_GETTER;
			TBCD operator/(const double rhs) const EL_GETTER;
			TBCD operator%(const double rhs) const EL_GETTER;
			TBCD operator+(const u64_t rhs) const EL_GETTER;
			TBCD operator-(const u64_t rhs) const EL_GETTER;
			TBCD operator*(const u64_t rhs) const EL_GETTER;
			TBCD operator/(const u64_t rhs) const EL_GETTER;
			TBCD operator%(const u64_t rhs) const EL_GETTER;
			TBCD operator+(const s64_t rhs) const EL_GETTER;
			TBCD operator-(const s64_t rhs) const EL_GETTER;
			TBCD operator*(const s64_t rhs) const EL_GETTER;
			TBCD operator/(const s64_t rhs) const EL_GETTER;
			TBCD operator%(const s64_t rhs) const EL_GETTER;
			TBCD operator+(const int rhs) const EL_GETTER;
			TBCD operator-(const int rhs) const EL_GETTER;
			TBCD operator*(const int rhs) const EL_GETTER;
			TBCD operator/(const int rhs) const EL_GETTER;
			TBCD operator%(const int rhs) const EL_GETTER;
			TBCD operator<<(const unsigned n_shift) const EL_GETTER;
			TBCD operator>>(const unsigned n_shift) const EL_GETTER;

			bool operator==(const TBCD& rhs) const EL_GETTER;
			bool operator!=(const TBCD& rhs) const EL_GETTER;
			bool operator>=(const TBCD& rhs) const EL_GETTER;
			bool operator<=(const TBCD& rhs) const EL_GETTER;
			bool operator> (const TBCD& rhs) const EL_GETTER;
			bool operator< (const TBCD& rhs) const EL_GETTER;
			bool operator==(const double rhs) const EL_GETTER;
			bool operator!=(const double rhs) const EL_GETTER;
			bool operator>=(const double rhs) const EL_GETTER;
			bool operator<=(const double rhs) const EL_GETTER;
			bool operator> (const double rhs) const EL_GETTER;
			bool operator< (const double rhs) const EL_GETTER;
			bool operator==(const u64_t rhs) const EL_GETTER;
			bool operator!=(const u64_t rhs) const EL_GETTER;
			bool operator>=(const u64_t rhs) const EL_GETTER;
			bool operator<=(const u64_t rhs) const EL_GETTER;
			bool operator> (const u64_t rhs) const EL_GETTER;
			bool operator< (const u64_t rhs) const EL_GETTER;
			bool operator==(const s64_t rhs) const EL_GETTER;
			bool operator!=(const s64_t rhs) const EL_GETTER;
			bool operator>=(const s64_t rhs) const EL_GETTER;
			bool operator<=(const s64_t rhs) const EL_GETTER;
			bool operator> (const s64_t rhs) const EL_GETTER;
			bool operator< (const s64_t rhs) const EL_GETTER;
			bool operator==(const int rhs) const EL_GETTER;
			bool operator!=(const int rhs) const EL_GETTER;
			bool operator>=(const int rhs) const EL_GETTER;
			bool operator<=(const int rhs) const EL_GETTER;
			bool operator> (const int rhs) const EL_GETTER;
			bool operator< (const int rhs) const EL_GETTER;

			// This function does NOT change `n_decimal`, it just rounds the extra digits so they become 0.
			// If `n_decimal_max` >= `n_decimal` then this function has no effect.
			void Round(const u8_t n_decimal_max, const math::ERoundingMode mode);

			// Sets all digits to 0.
			void SetZero() noexcept;

			// returns true if all digits are 0 (or were not allocated)
			bool IsZero() const noexcept EL_GETTER;

			double ToDouble() const EL_GETTER;

			// Removes the decimal digits and only returns the integer part.
			s64_t ToSignedInt() const EL_GETTER;
			u64_t ToUnsignedInt() const EL_GETTER;
			TBCD ToBCDInt() const EL_GETTER;

			explicit operator double() const EL_GETTER;
			explicit operator s64_t() const EL_GETTER;
			explicit operator u64_t() const EL_GETTER;

			// Returns true if `base`, `n_decimal` and `n_integer` are equal to `rhs`.
			bool HasSameSpecs(const TBCD& rhs) const EL_GETTER;

			// Returns a negative value if `this` < `rhs`.
			// Returns a positive value if `this` > `rhs`.
			// Returns zero if `this` == `rhs`.
			int Compare(const TBCD& rhs, const bool absolute = false) const EL_GETTER;

			TBCD(TBCD&&) = default;
			TBCD(const TBCD&);

			// if `n_integer` and/or `n_decimal` is `AUTO_DETECT` then their value will be computed based on
			// the given base and the input datatype. For integer types `n_decimal` will be 0.
			// The base can be 0 in which case TBCD behaves like an IEEE integer with optional decimal digits and
			// adjustable size/precision. If `v` is 0 then no digits are allocated until a non-zero value is assigned.
			TBCD(const TBCD&  v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = AUTO_DETECT);
			TBCD(const u8_t   v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = 0);
			TBCD(const s8_t   v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = 0);
			TBCD(const u16_t  v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = 0);
			TBCD(const s16_t  v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = 0);
			TBCD(const u32_t  v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = 0);
			TBCD(const s32_t  v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = 0);
			TBCD(const u64_t  v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = 0);
			TBCD(const s64_t  v, const digit_t base, const u8_t n_integer = AUTO_DETECT, const u8_t n_decimal = 0);
			// For float input types the auto-detection does not work and `n_integer` and `n_decimal` must be manually set.
			TBCD(const float  v, const digit_t base, const u8_t n_integer, const u8_t n_decimal);
			TBCD(const double v, const digit_t base, const u8_t n_integer, const u8_t n_decimal);

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

			~TBCD();

			static const TBCD INVALID;

			// First digit in str is considered least significant value, while last position is most significant value (reversed to what humans do).
			static TBCD FromString(const text::string::TString& str, const text::string::TString& symbols, const text::encoding::TUTF32 decimal_seperator = '.');
	};
}
