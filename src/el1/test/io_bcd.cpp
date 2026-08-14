#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <utility>
#include <el1/error.hpp>
#include <el1/io_types.hpp>
#include <el1/io_bcd.hpp>
#include <el1/io_text_string.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::error;
	using namespace el1::io::types;
	using namespace el1::io::bcd;
	using namespace el1::io::text::encoding;
	using namespace el1::io::text::string;
	using namespace el1::math;

	TBCD Parse(const char* const text, const TStringView symbols = DECIMAL_SYMBOLS)
	{
		return TBCD::FromString(TString(text).Reverse(), symbols);
	}

	void ExpectValue(const TBCD& actual, const char* const expected)
	{
		EXPECT_EQ(actual, Parse(expected));
	}

	struct TBCDTestAccess : TBCD
	{
		using TBCD::TBCD;
		using TBCD::AbsMul;
		using TBCD::ConvertBCD;
	};

	TBCD FromScaled(const s64_t scaled, const digit_t base, const u8_t n_integer = 8, const u8_t n_decimal = 2)
	{
		TBCD value(0, base, n_integer, n_decimal);
		u64_t magnitude = scaled < 0 ? (u64_t)(0U - (u64_t)scaled) : (u64_t)scaled;
		const unsigned radix = value.Radix();
		for(unsigned i = 0; magnitude != 0 && i < (unsigned)n_integer + (unsigned)n_decimal; i++)
		{
			value.Digit((int)i - (int)n_decimal, (digit_t)(magnitude % radix));
			magnitude /= radix;
		}
		EXPECT_EQ(magnitude, 0U);
		value.IsNegative(scaled < 0 && !value.IsZero());
		return value;
	}

	TEST(io_bcd, ConstructIntegerAndAutoDetect)
	{
		const TBCD zero(0, 10, 2, 0);
		EXPECT_EQ(zero.Base(), 10U);
		EXPECT_EQ(zero.Radix(), 10U);
		EXPECT_EQ(zero.CountInteger(), 2U);
		EXPECT_EQ(zero.CountDecimal(), 0U);
		EXPECT_TRUE(zero.IsZero());

		const TBCD positive((u16_t)0xabcd, 16, 5, 0);
		EXPECT_EQ(positive.Digit(0), 0xd);
		EXPECT_EQ(positive.Digit(1), 0xc);
		EXPECT_EQ(positive.Digit(2), 0xb);
		EXPECT_EQ(positive.Digit(3), 0xa);
		EXPECT_EQ(positive.Digit(4), 0);

		const TBCD negative((s64_t)-15, 10, 2, 0);
		EXPECT_TRUE(negative.IsNegative());
		EXPECT_EQ(negative.Digit(0), 5);
		EXPECT_EQ(negative.Digit(1), 1);

		const TBCD auto_u64(std::numeric_limits<u64_t>::max(), 10);
		EXPECT_EQ(auto_u64.CountInteger(), 20U);
		EXPECT_EQ(auto_u64.ToUnsignedInt(), std::numeric_limits<u64_t>::max());

		const TBCD min_s64(std::numeric_limits<s64_t>::min(), 10, 20, 0);
		EXPECT_TRUE(min_s64.IsNegative());
		EXPECT_EQ(min_s64.ToSignedInt(), std::numeric_limits<s64_t>::min());
		EXPECT_EQ(min_s64.ToUnsignedInt(), 9223372036854775808ULL);
	}

	TEST(io_bcd, ConstructFloat)
	{
		const TBCD positive(10.751, 10, 2, 3);
		ExpectValue(positive, "10.750");
		EXPECT_FALSE(positive.IsNegative());

		const TBCD negative(-10.751, 10, 2, 3);
		ExpectValue(negative, "-10.750");
		EXPECT_TRUE(negative.IsNegative());

		const TBCD truncated(17.356, 10, 8, 2);
		ExpectValue(truncated, "17.35");

		const TBCD huge(std::numeric_limits<double>::max(), 10, 4, 8);
		for(const digit_t digit : huge.Digits())
			EXPECT_LT(digit, 10U);

		const TBCD positive_infinity(std::numeric_limits<double>::infinity(), 10, 2, 2);
		EXPECT_TRUE(positive_infinity.IsInfinity());
		EXPECT_FALSE(positive_infinity.IsNegative());
		EXPECT_TRUE(std::isinf(positive_infinity.ToDouble()));

		const TBCD negative_infinity(-std::numeric_limits<double>::infinity(), 10, 2, 2);
		EXPECT_TRUE(negative_infinity.IsInfinity());
		EXPECT_TRUE(negative_infinity.IsNegative());
		EXPECT_LT(negative_infinity.ToDouble(), 0.0);

		const TBCD nan(std::numeric_limits<double>::quiet_NaN(), 10, 2, 2);
		EXPECT_TRUE(nan.IsNaN());
		EXPECT_TRUE(std::isnan(nan.ToDouble()));
	}

	TEST(io_bcd, FloatingConstructionUsesExactIEEE754Value)
	{
		const TBCD decimal_double(0.1, 10, 1, 60);
		ExpectValue(decimal_double, "0.100000000000000005551115123125782702118158340454101562500000");
		EXPECT_EQ(decimal_double, 0.1);
		EXPECT_GT(decimal_double, Parse("0.1"));

		const TBCD decimal_float((float)0.1, 10, 1, 30);
		ExpectValue(decimal_float, "0.100000001490116119384765625000");

		const TBCD exact_fraction(1.25, 10, 2, 30);
		ExpectValue(exact_fraction, "1.250000000000000000000000000000");
		EXPECT_EQ(exact_fraction, 1.25);

		const TBCD smallest(std::numeric_limits<double>::denorm_min(), 2, 1, 1074);
		EXPECT_EQ(smallest.Digit(-1074), 1U);
		EXPECT_EQ(smallest.CountSignificantDecimalDigits(), 1074U);
		EXPECT_EQ(smallest, std::numeric_limits<double>::denorm_min());
	}

	TEST(io_bcd, RequiredDigits)
	{
		EXPECT_EQ(TBCD::RequiredDigits(10, 2, 8), 3U);
		EXPECT_EQ(TBCD::RequiredDigits(16, 2, 16), 4U);
		EXPECT_EQ(TBCD::RequiredDigits(2, 16, 4), 16U);
		EXPECT_EQ(TBCD::RequiredDigits(0, 2, 64), 8U);
		EXPECT_EQ(TBCD::RequiredDigits(0, 2, 2040), 255U);
		EXPECT_EQ(TBCD::RequiredDigits(2, 0, 2), 16U);
		EXPECT_EQ(TBCD::RequiredDigits(10, 10, 0), 0U);
		EXPECT_THROW({ volatile usys_t n = TBCD::RequiredDigits(1, 10, 1); (void)n; }, TInvalidArgumentException);
		EXPECT_THROW({ volatile usys_t n = TBCD::RequiredDigits(10, 1, 1); (void)n; }, TInvalidArgumentException);
		EXPECT_EQ(TBCD::RequiredDigits(2, 0, 255), 2040U);
		EXPECT_EQ(TBCD::RequiredDigits(0, 2, 2041), 256U);
	}

	TEST(io_bcd, Base256)
	{
		const TBCD value((u32_t)0x01020304U, 0, 4, 0);
		EXPECT_EQ(value.Base(), 0U);
		EXPECT_EQ(value.Radix(), 256U);
		EXPECT_EQ(value.Digit(0), 0x04);
		EXPECT_EQ(value.Digit(1), 0x03);
		EXPECT_EQ(value.Digit(2), 0x02);
		EXPECT_EQ(value.Digit(3), 0x01);
		EXPECT_EQ(value.ToUnsignedInt(), 0x01020304U);

		TBCD sum(0, 0, 4, 0);
		TBCD::Add(sum, TBCD(0xff, 0, 4, 0), TBCD(2, 0, 4, 0));
		EXPECT_EQ(sum, 0x101);
	}

	TEST(io_bcd, DigitAccessAndZeroCache)
	{
		TBCD value(0, 10, 3, 2);
		EXPECT_TRUE(value.IsZero());
		EXPECT_FALSE(value.Digit(-3, 1));
		EXPECT_FALSE(value.Digit(3, 1));
		EXPECT_THROW(value.Digit(0, 10), TInvalidArgumentException);
		EXPECT_TRUE(value.Digit(-2, 4));
		EXPECT_TRUE(value.Digit(0, 7));
		EXPECT_FALSE(value.IsZero());
		ExpectValue(value, "7.04");
		EXPECT_TRUE(value.Digit(-2, 0));
		EXPECT_TRUE(value.Digit(0, 0));
		EXPECT_TRUE(value.IsZero());
	}

	TEST(io_bcd, InvalidValueIsSafeToInspect)
	{
		EXPECT_TRUE(TBCD::INVALID.IsInvalid());
		EXPECT_TRUE(TBCD::INVALID.IsZero());
		EXPECT_EQ(TBCD::INVALID.Digits().Count(), 0U);
		EXPECT_EQ(TBCD::INVALID.IntegerDigits().Count(), 0U);
		EXPECT_EQ(TBCD::INVALID.DecimalDigits().Count(), 0U);
	}

	TEST(io_bcd, CopyAndMoveSmallDynamicStorage)
	{
		TBCD source = Parse("-1234.5678");
		TBCD copy(source);
		EXPECT_EQ(copy, source);

		TBCD moved(std::move(source));
		ExpectValue(moved, "-1234.5678");
		EXPECT_TRUE(source.IsZero());

		TBCD assigned(0, 10, 4, 4);
		assigned = std::move(moved);
		ExpectValue(assigned, "-1234.5678");
		EXPECT_TRUE(moved.IsZero());
	}

	TEST(io_bcd, CopyAndMoveLargeDynamicStorage)
	{
		const TBCD reference = Parse("-12345678901234567890.12345678901234567890");
		TBCD copy(reference);
		EXPECT_EQ(copy, reference);

		TBCD source(reference);
		TBCD moved(std::move(source));
		EXPECT_EQ(moved, reference);
		EXPECT_TRUE(source.IsZero());

		TBCD destination(0, moved.Base(), moved.CountInteger(), moved.CountDecimal());
		destination = std::move(moved);
		EXPECT_EQ(destination, reference);
		EXPECT_TRUE(moved.IsZero());
	}

	TEST(io_bcd, MoveAssignmentConvertsDifferentConfigurations)
	{
		TBCD decimal = Parse("123.75");
		TBCD binary(0, 2, 16, 4);
		binary = std::move(decimal);
		EXPECT_EQ(binary, Parse("1111011.11", TString("01")));
		EXPECT_TRUE(decimal.IsZero());

		TBCD external = Parse("12345678901234567890.125");
		TBCD compact(0, 10, 4, 1);
		compact = std::move(external);
		ExpectValue(compact, "7890.1");
		EXPECT_TRUE(external.IsZero());
	}

	TEST(io_bcd, NegativeZeroIsPreservedExplicitly)
	{
		TBCD value(0, 10, 2, 2);
		value.IsNegative(true);
		EXPECT_TRUE(value.IsZero());
		EXPECT_TRUE(value.IsNegative());
		EXPECT_EQ(value, TBCD(0, 10, 2, 2));

		TBCD copy(value);
		EXPECT_TRUE(copy.IsNegative());
		TBCD moved(std::move(copy));
		EXPECT_TRUE(moved.IsNegative());
		EXPECT_TRUE(moved.IsZero());
	}

	TEST(io_bcd, CompareDifferentPrecisionAndBases)
	{
		EXPECT_LT(Parse("1.20"), Parse("1.25"));
		EXPECT_GT(Parse("-1.20"), Parse("-1.25"));
		EXPECT_EQ(Parse("12.50"), Parse("12.5"));

		const TBCD binary = Parse("1010.1", TString("01"));
		const TBCD decimal = Parse("10.5");
		EXPECT_EQ(binary, decimal);
		EXPECT_EQ(binary.Compare(decimal), 0);

		const TBCD base_five = Parse("12.3", TString("01234"));
		const TBCD base_ten = Parse("7.6");
		EXPECT_EQ(base_five, base_ten);
	}

	TEST(io_bcd, ComparePrimitiveDoubleDoesNotQuantizeRhs)
	{
		const TBCD integer(3, 10, 8, 0);
		EXPECT_EQ(integer, 3.0);
		EXPECT_NE(integer, 3.5);
		EXPECT_LT(integer, 3.5);
		EXPECT_GT(integer, 2.5);

		const TBCD decimal = Parse("17.28");
		// 17.28 is slightly larger than exact decimal 17.28 as an IEEE-754 double.
		EXPECT_LT(decimal, 17.28);
		EXPECT_NE(decimal, 17.28);
		EXPECT_FALSE(decimal == std::numeric_limits<double>::quiet_NaN());
		EXPECT_TRUE(decimal != std::numeric_limits<double>::quiet_NaN());
		EXPECT_LT(decimal, std::numeric_limits<double>::infinity());
		EXPECT_GT(decimal, -std::numeric_limits<double>::infinity());
	}

	TEST(io_bcd, AddSubtractSignsAndAliasing)
	{
		ExpectValue(TBCD(122, 10, 8, 0) + TBCD(277, 10, 8, 0), "399");
		ExpectValue(TBCD(122, 10, 8, 0) - TBCD(277, 10, 8, 0), "-155");
		ExpectValue(TBCD(-122, 10, 8, 0) + TBCD(277, 10, 8, 0), "155");
		ExpectValue(TBCD(-122, 10, 8, 0) - TBCD(277, 10, 8, 0), "-399");

		TBCD lhs = Parse("12.34");
		const TBCD rhs = Parse("5.67");
		TBCD::Add(lhs, lhs, rhs);
		ExpectValue(lhs, "18.01");
		TBCD::Subtract(lhs, lhs, rhs);
		ExpectValue(lhs, "12.34");

		TBCD rhs_alias = Parse("15.67");
		const TBCD lhs_const = Parse("12.34");
		TBCD::Add(rhs_alias, lhs_const, rhs_alias);
		ExpectValue(rhs_alias, "28.01");
	}

	TEST(io_bcd, AddSubtractUsesDiscardedFractionForCarryAndBorrow)
	{
		const TBCD a = Parse("0.06");
		const TBCD b = Parse("0.06");
		TBCD sum(0, 10, 2, 1);
		EXPECT_EQ(TBCD::Add(sum, a, b), 0);
		ExpectValue(sum, "0.1");

		const TBCD one = Parse("1.04");
		TBCD difference(0, 10, 2, 1);
		EXPECT_EQ(TBCD::Subtract(difference, one, b), 0);
		ExpectValue(difference, "0.9");
	}

	TEST(io_bcd, AddSubtractReportsIntegerOverflow)
	{
		TBCD out(0, 10, 2, 0);
		EXPECT_NE(TBCD::Add(out, TBCD(99, 10, 2, 0), TBCD(2, 10, 2, 0)), 0);
		ExpectValue(out, "1");

		EXPECT_NE(TBCD::Subtract(out, TBCD(-99, 10, 2, 0), TBCD(2, 10, 2, 0)), 0);
		ExpectValue(out, "-1");
	}

	TEST(io_bcd, PrimitiveOperatorsAndAssignments)
	{
		TBCD value = Parse("10.50");
		value += 2;
		ExpectValue(value, "12.50");
		value -= (s64_t)3;
		ExpectValue(value, "9.50");
		value *= (u64_t)2;
		ExpectValue(value, "19.00");
		value /= 4;
		ExpectValue(value, "4.75");
		value %= 2;
		ExpectValue(value, "0.01");

		value = -7;
		ExpectValue(value, "-7.00");
		value = (u64_t)42;
		ExpectValue(value, "42.00");
		value = 3.25;
		ExpectValue(value, "3.25");

		EXPECT_TRUE(value > 3);
		EXPECT_TRUE(value < (u64_t)4);
		EXPECT_TRUE(value >= (s64_t)3);
	}

	TEST(io_bcd, CrossBaseArithmetic)
	{
		const TBCD binary = Parse("1010.1", TString("01"));
		const TBCD decimal = Parse("2.25");
		TBCD out(0, 10, 4, 2);

		TBCD::Add(out, binary, decimal);
		ExpectValue(out, "12.75");
		TBCD::Subtract(out, binary, decimal);
		ExpectValue(out, "8.25");
		TBCD::Multiply(out, binary, decimal);
		ExpectValue(out, "23.62");
		TBCD::Divide(out, binary, decimal);
		ExpectValue(out, "4.66");
	}

	TEST(io_bcd, CrossBaseArithmeticProperty)
	{
		const digit_t bases[] = {2, 3, 10, 16, 0};
		const s64_t lhs_values[] = {-37, -11, -1, 0, 1, 13, 41};
		const s64_t rhs_values[] = {-29, -3, 1, 7, 31};

		for(const digit_t lhs_base : bases)
			for(const digit_t rhs_base : bases)
				for(const digit_t out_base : bases)
				{
					const s64_t lhs_radix = lhs_base == 0 ? 256 : lhs_base;
					const s64_t rhs_radix = rhs_base == 0 ? 256 : rhs_base;
					const s64_t out_radix = out_base == 0 ? 256 : out_base;
					const s64_t lhs_scale = lhs_radix * lhs_radix;
					const s64_t rhs_scale = rhs_radix * rhs_radix;
					const s64_t out_scale = out_radix * out_radix;

					for(const s64_t lhs_scaled : lhs_values)
						for(const s64_t rhs_scaled : rhs_values)
						{
							const TBCD lhs = FromScaled(lhs_scaled, lhs_base);
							const TBCD rhs = FromScaled(rhs_scaled, rhs_base);
							TBCD out(0, out_base, 32, 2);

							const s64_t add_scaled = (lhs_scaled * rhs_scale + rhs_scaled * lhs_scale) * out_scale / (lhs_scale * rhs_scale);
							TBCD::Add(out, lhs, rhs);
							EXPECT_EQ(out, FromScaled(add_scaled, out_base, 32, 2));

							const s64_t subtract_scaled = (lhs_scaled * rhs_scale - rhs_scaled * lhs_scale) * out_scale / (lhs_scale * rhs_scale);
							TBCD::Subtract(out, lhs, rhs);
							EXPECT_EQ(out, FromScaled(subtract_scaled, out_base, 32, 2));

							const s64_t multiply_scaled = lhs_scaled * rhs_scaled * out_scale / (lhs_scale * rhs_scale);
							TBCD::Multiply(out, lhs, rhs);
							EXPECT_EQ(out, FromScaled(multiply_scaled, out_base, 32, 2));

							const s64_t compare_left = lhs_scaled * rhs_scale;
							const s64_t compare_right = rhs_scaled * lhs_scale;
							EXPECT_EQ(lhs.Compare(rhs), compare_left < compare_right ? -1 : compare_left > compare_right ? 1 : 0);

							if(rhs_scaled != 0)
							{
								const s64_t quotient_scaled = lhs_scaled * rhs_scale * out_scale / (rhs_scaled * lhs_scale);
								const s64_t remainder_scaled = (lhs_scaled * out_scale * rhs_scale - quotient_scaled * rhs_scaled * lhs_scale) / (lhs_scale * rhs_scale);
								const TBCD remainder = TBCD::Divide(out, lhs, rhs);
								EXPECT_EQ(out, FromScaled(quotient_scaled, out_base, 32, 2));
								EXPECT_EQ(remainder, FromScaled(remainder_scaled, out_base, 32, 2));
							}
						}
				}
	}

	TEST(io_bcd, MultiplyUsesOutputPrecision)
	{
		const TBCD a = Parse("-1234.17");
		const TBCD b = Parse("5678.35");
		TBCD out(0, 10, 8, 2);
		TBCD::Multiply(out, a, b);
		ExpectValue(out, "-7008049.21");

		TBCD alias = Parse("12.34");
		const TBCD factor = Parse("5.6");
		TBCD::Multiply(alias, alias, factor);
		ExpectValue(alias, "69.10");
	}

	TEST(io_bcd, DivideUsesOutputPrecisionAndSpecialZeroSemantics)
	{
		const TBCD lhs(35, 10, 8, 0);
		const TBCD rhs(10, 10, 8, 0);
		ExpectValue(lhs / rhs, "3");

		TBCD precise(0, 10, 8, 4);
		TBCD remainder = TBCD::Divide(precise, lhs, rhs);
		ExpectValue(precise, "3.5000");
		EXPECT_TRUE(remainder.IsZero());

		TBCD out(0, 10, 4, 2);
		TBCD remainder_zero = TBCD::Divide(out, TBCD(0, 10, 4, 2), TBCD(0, 10, 4, 2));
		EXPECT_TRUE(out.IsNaN());
		EXPECT_TRUE(remainder_zero.IsNaN());

		TBCD remainder_infinity = TBCD::Divide(out, TBCD(1, 10, 4, 2), TBCD(0, 10, 4, 2));
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_FALSE(out.IsNegative());
		EXPECT_TRUE(remainder_infinity.IsNaN());
	}

	TEST(io_bcd, ModuloReturnsResidualAtConfiguredPrecision)
	{
		ExpectValue(TBCD(27, 10, 8, 0) % TBCD(12, 10, 8, 0), "3");
		ExpectValue(Parse("27.0") % Parse("12.0"), "0.6");
		ExpectValue(Parse("5.5") % Parse("2.0"), "0.1");
		ExpectValue(Parse("-27.0") % Parse("12.0"), "-0.6");
		ExpectValue(Parse("27.0") % Parse("-12.0"), "0.6");
		ExpectValue(Parse("-27.0") % Parse("-12.0"), "-0.6");
	}

	TEST(io_bcd, DivideResidualPreservesIdentityAtOutputPrecision)
	{
		for(const char* const lhs_text : {"27.0", "-27.0"})
			for(const char* const rhs_text : {"12.0", "-12.0"})
			{
				const TBCD lhs = Parse(lhs_text);
				const TBCD rhs = Parse(rhs_text);
				TBCD quotient(0, 10, 4, 1);
				const TBCD remainder = TBCD::Divide(quotient, lhs, rhs);
				EXPECT_EQ(quotient * rhs + remainder, lhs);
				EXPECT_EQ(remainder.IsNegative(), lhs.IsNegative());
			}
	}

	TEST(io_bcd, RoundNearestAndNearestEven)
	{
		TBCD away = Parse("1.25");
		away.Round(1, ERoundingMode::TO_NEAREST);
		ExpectValue(away, "1.3");

		TBCD away_negative = Parse("-1.25");
		away_negative.Round(1, ERoundingMode::TO_NEAREST);
		ExpectValue(away_negative, "-1.3");

		TBCD even_down = Parse("1.25");
		even_down.Round(1, ERoundingMode::TO_NEAREST_EVEN);
		ExpectValue(even_down, "1.2");

		TBCD even_up = Parse("1.35");
		even_up.Round(1, ERoundingMode::TO_NEAREST_EVEN);
		ExpectValue(even_up, "1.4");
	}

	TEST(io_bcd, RoundDirectedModes)
	{
		TBCD towards = Parse("-1.21");
		towards.Round(1, ERoundingMode::TOWARDS_ZERO);
		ExpectValue(towards, "-1.2");

		TBCD away = Parse("-1.21");
		away.Round(1, ERoundingMode::AWAY_FROM_ZERO);
		ExpectValue(away, "-1.3");

		TBCD down = Parse("-1.21");
		down.Round(1, ERoundingMode::DOWNWARD);
		ExpectValue(down, "-1.3");

		TBCD up = Parse("-1.21");
		up.Round(1, ERoundingMode::UPWARD);
		ExpectValue(up, "-1.2");

		TBCD positive_down = Parse("1.21");
		positive_down.Round(1, ERoundingMode::DOWNWARD);
		ExpectValue(positive_down, "1.2");
	}

	TEST(io_bcd, RoundWorksForOddBases)
	{
		TBCD below_half(0, 5, 2, 1);
		below_half.Digit(-1, 2);
		below_half.Round(0, ERoundingMode::TO_NEAREST);
		EXPECT_TRUE(below_half.IsZero());

		TBCD above_half(0, 5, 2, 1);
		above_half.Digit(-1, 3);
		above_half.Round(0, ERoundingMode::TO_NEAREST);
		EXPECT_EQ(above_half, 1);
	}

	TEST(io_bcd, RoundPropagatesCarry)
	{
		TBCD value = Parse("099.99");
		value.Round(1, ERoundingMode::TO_NEAREST);
		ExpectValue(value, "100.0");
	}

	TEST(io_bcd, StochasticRoundProducesAdjacentValue)
	{
		for(unsigned i = 0; i < 32; i++)
		{
			TBCD value = Parse("1.25");
			value.Round(1, ERoundingMode::STOCHASTIC);
			EXPECT_TRUE(value == Parse("1.2") || value == Parse("1.3"));
		}
	}

	TEST(io_bcd, Shift)
	{
		TBCD value = Parse("012.34");
		value <<= 1;
		ExpectValue(value, "123.40");
		value >>= 2;
		ExpectValue(value, "1.23");
		value.Shift(-1);
		ExpectValue(value, "0.12");
		value.Shift(100);
		EXPECT_TRUE(value.IsZero());
	}

	TEST(io_bcd, ConvertAcrossBases)
	{
		const TBCD decimal = Parse("10.5");
		const TBCD binary(decimal, 2, 8, 4);
		EXPECT_EQ(binary, decimal);
		EXPECT_EQ(binary.Digit(3), 1);
		EXPECT_EQ(binary.Digit(1), 1);
		EXPECT_EQ(binary.Digit(-1), 1);

		const TBCD back(binary, 10, 4, 2);
		ExpectValue(back, "10.50");
	}

	TEST(io_bcd, ConvertAcrossBasesTruncatesOnlyAtDestinationPrecision)
	{
		const TBCD decimal = Parse("0.10");
		const TBCD binary(decimal, 2, 2, 8);
		EXPECT_EQ(binary.Digit(-1), 0);
		EXPECT_EQ(binary.Digit(-2), 0);
		EXPECT_EQ(binary.Digit(-3), 0);
		EXPECT_EQ(binary.Digit(-4), 1);
		EXPECT_LE(binary.ToDouble(), 0.1);
		EXPECT_GT(binary.ToDouble(), 0.09);
	}

	TEST(io_bcd, ConfigurationReferencePreserves255DigitPrecision)
	{
		const TBCD conf(0.0, 10, 255, 255);
		const TBCD integer_value(1, conf);
		const TBCD decimal_value(1.25, conf);

		EXPECT_EQ(integer_value.CountInteger(), 255U);
		EXPECT_EQ(integer_value.CountDecimal(), 255U);
		EXPECT_EQ(integer_value, 1);
		EXPECT_EQ(decimal_value.CountInteger(), 255U);
		EXPECT_EQ(decimal_value.CountDecimal(), 255U);
		EXPECT_EQ(decimal_value, 1.25);
		EXPECT_LE(decimal_value, 1.25);
		EXPECT_GE(decimal_value, 1.25);
		EXPECT_FALSE(decimal_value < 1.25);
		EXPECT_FALSE(decimal_value > 1.25);
		EXPECT_EQ(decimal_value.Digit(-1), 2);
		EXPECT_EQ(decimal_value.Digit(-2), 5);
		for(usys_t i = 3; i <= 255; i++)
			EXPECT_EQ(decimal_value.Digit(-(ssys_t)i), 0) << "unexpected floating-point noise at decimal digit " << i;
	}

	TEST(io_bcd, CrossBaseHighPrecisionFractions)
	{
		const TBCD one_third = Parse("0.1", TString("012"));
		const TBCD two_thirds = Parse("0.2", TString("012"));

		TBCD lower(0.0, 10, 1, 255);
		TBCD upper(0.0, 10, 1, 255);
		for(unsigned i = 1; i <= 255; i++)
		{
			lower.Digit(-(int)i, 3);
			upper.Digit(-(int)i, 3);
		}
		upper.Digit(-255, 4);

		EXPECT_LT(lower, one_third);
		EXPECT_GT(upper, one_third);
		TBCD negative_lower(lower);
		TBCD negative_third(one_third);
		negative_lower.IsNegative(true);
		negative_third.IsNegative(true);
		EXPECT_GT(negative_lower, negative_third);

		TBCD converted(0.0, 10, 1, 255);
		converted = one_third;
		EXPECT_EQ(converted, lower);

		TBCD sum(0.0, 10, 2, 255);
		TBCD::Add(sum, one_third, two_thirds);
		EXPECT_EQ(sum, 1);

		TBCD product(0.0, 10, 2, 255);
		TBCD::Multiply(product, one_third, TBCD(3, 10, 1, 0));
		EXPECT_EQ(product, 1);

		TBCD quotient(0.0, 10, 2, 255);
		const TBCD remainder = TBCD::Divide(quotient, TBCD(1, 10, 1, 0), one_third);
		EXPECT_EQ(quotient, 3);
		EXPECT_TRUE(remainder.IsZero());
	}

	TEST(io_bcd, LongDivisionPropertyAcrossRadices)
	{
		u64_t state = 0x9e3779b97f4a7c15ULL;
		for(const digit_t base : {digit_t(2), digit_t(3), digit_t(10), digit_t(16), digit_t(0)})
		{
			for(unsigned iteration = 0; iteration < 256; iteration++)
			{
				state = state * 6364136223846793005ULL + 1442695040888963407ULL;
				const u64_t lhs_value = (state >> 16) % 4000000000ULL;
				state = state * 6364136223846793005ULL + 1442695040888963407ULL;
				const u64_t rhs_value = 257ULL + ((state >> 24) % 1000000ULL);

				const TBCD lhs(lhs_value, base, 64, 0);
				const TBCD rhs(rhs_value, base, 64, 0);
				TBCD quotient(0ULL, base, 64, 0);
				const TBCD remainder = TBCD::Divide(quotient, lhs, rhs);

				EXPECT_EQ(quotient.ToUnsignedInt(), lhs_value / rhs_value);
				EXPECT_EQ(remainder.ToUnsignedInt(), lhs_value % rhs_value);
			}
		}
	}

	TEST(io_bcd, CrossBaseLongDivisionProperty)
	{
		u64_t state = 0xd1b54a32d192ed03ULL;
		for(unsigned iteration = 0; iteration < 256; iteration++)
		{
			state = state * 2862933555777941757ULL + 3037000493ULL;
			const u64_t lhs_value = (state >> 17) % 3000000000ULL;
			state = state * 2862933555777941757ULL + 3037000493ULL;
			const u64_t rhs_value = 257ULL + ((state >> 23) % 1000000ULL);

			const TBCD lhs(lhs_value, 3, 64, 0);
			const TBCD rhs(rhs_value, 10, 32, 0);
			TBCD quotient(0ULL, 16, 32, 0);
			const TBCD remainder = TBCD::Divide(quotient, lhs, rhs);

			EXPECT_EQ(quotient.ToUnsignedInt(), lhs_value / rhs_value);
			EXPECT_EQ(remainder.ToUnsignedInt(), lhs_value % rhs_value);
		}
	}

	TEST(io_bcd, DivideAt255DecimalPrecision)
	{
		const TBCD lhs(1, 10, 2, 0);
		const TBCD rhs(7, 10, 2, 0);
		TBCD quotient(0.0, 10, 2, 255);
		const TBCD remainder = TBCD::Divide(quotient, lhs, rhs);

		static const digit_t cycle[] = {1, 4, 2, 8, 5, 7};
		for(unsigned i = 1; i <= 255; i++)
			EXPECT_EQ(quotient.Digit(-(int)i), cycle[(i - 1U) % 6U]);

		TBCD expected_lhs(0.0, 10, 2, 255);
		expected_lhs = lhs;
		EXPECT_EQ(quotient * rhs + remainder, expected_lhs);
		EXPECT_EQ(remainder.Digit(-255), 6);
		EXPECT_EQ(remainder.CountSignificantDecimalDigits(), 255U);
	}

	TEST(io_bcd, FromStringSignsAndLargeStorage)
	{
		ExpectValue(TBCD::FromString(TString("+17.28").Reverse(), DECIMAL_SYMBOLS), "17.28");
		ExpectValue(TBCD::FromString(TString("-17.28").Reverse(), DECIMAL_SYMBOLS), "-17.28");
		ExpectValue(TBCD::FromString(TString("17.28+").Reverse(), DECIMAL_SYMBOLS), "17.28");
		ExpectValue(TBCD::FromString(TString("17.28-").Reverse(), DECIMAL_SYMBOLS), "-17.28");

		const TBCD long_value = Parse("123456789012345678901234567890.12345678901234567890");
		EXPECT_EQ(long_value.CountInteger(), 30U);
		EXPECT_EQ(long_value.CountDecimal(), 20U);
		ExpectValue(long_value, "123456789012345678901234567890.12345678901234567890");
	}

	TEST(io_bcd, FromStringMSDAndFixedIntegerConversion)
	{
		ExpectValue(TBCD::FromStringMSD(U"+17.28", DECIMAL_SYMBOLS), "17.28");
		ExpectValue(TBCD::FromStringMSD(U"-17.28", (digit_t)10), "-17.28");
		EXPECT_EQ(TBCD::FromStringMSD(U"fF", (digit_t)16), 255);
		EXPECT_NO_THROW((void)TBCD::FromStringMSD(U"1x", (digit_t)10));
		EXPECT_NO_THROW((void)TBCD::FromStringMSD(U"1.2.3", (digit_t)10));
		EXPECT_NO_THROW((void)TBCD::FromStringMSD(U"12", TStringView(U"0012345678")));
		ExpectValue(TBCD::FromStringMSDShifted(U"1.25", (digit_t)10, 3), "1250");
		ExpectValue(TBCD::FromStringMSDShifted(U"123", (digit_t)10, -5), "0.00123");
		EXPECT_DOUBLE_EQ(TBCD::ParseDoubleMSD(U"-12345.6789", (digit_t)10), TBCD::FromStringMSD(U"-12345.6789", (digit_t)10).ToDouble());
		EXPECT_DOUBLE_EQ(TBCD::ParseDoubleMSD(U"-1.23456789", (digit_t)10, 4), TBCD::FromStringMSDShifted(U"-1.23456789", (digit_t)10, 4).ToDouble());

		TFixedBCD<20, 20, 0, 10> maximum((u64_t)std::numeric_limits<s64_t>::max());
		s64_t signed_value = 0;
		EXPECT_TRUE(maximum.TryToInteger(signed_value));
		EXPECT_EQ(signed_value, std::numeric_limits<s64_t>::max());

		TFixedBCD<20, 20, 0, 10> too_large((u64_t)std::numeric_limits<s64_t>::max() + 1U);
		EXPECT_FALSE(too_large.TryToInteger(signed_value));

		TFixedBCD<1, 1, 0, 10> boolean(1);
		bool bool_value = false;
		EXPECT_TRUE(boolean.TryToInteger(bool_value));
		EXPECT_TRUE(bool_value);
		boolean.Digit(0, 2);
		EXPECT_FALSE(boolean.TryToInteger(bool_value));
	}

	TEST(io_bcd, FromStringRejectsMalformedInput)
	{
		EXPECT_TRUE(TBCD::FromString(TString(), DECIMAL_SYMBOLS).IsInvalid());
		EXPECT_THROW(TBCD::FromString(TString("+").Reverse(), DECIMAL_SYMBOLS), TInvalidArgumentException);
		EXPECT_THROW(TBCD::FromString(TString("1.2.3").Reverse(), DECIMAL_SYMBOLS), TInvalidArgumentException);
		EXPECT_THROW(TBCD::FromString(TString("1x").Reverse(), DECIMAL_SYMBOLS), TInvalidArgumentException);
		EXPECT_THROW(TBCD::FromString(TString("+-1").Reverse(), DECIMAL_SYMBOLS), TInvalidArgumentException);
		EXPECT_THROW(TBCD::FromString(TString("1").Reverse(), TString("00123456789")), TInvalidArgumentException);
		EXPECT_THROW(TBCD::FromString(TString("1").Reverse(), TString("01.")), TInvalidArgumentException);
		EXPECT_THROW(TBCD::FromString(TString("1").Reverse(), TString("0-1")), TInvalidArgumentException);
	}

	TEST(io_bcd, FromStringSupports256Symbols)
	{
		TString symbols;
		for(u32_t i = 0; i < 256; i++)
			symbols += char32_t(0x1000U + i);

		TString digits;
		digits += symbols[5];
		digits += symbols[1];
		const TBCD value = TBCD::FromString(digits, symbols);
		EXPECT_EQ(value.Base(), 0U);
		EXPECT_EQ(value.Radix(), 256U);
		EXPECT_EQ(value, 261);
	}

	TEST(io_bcd, RandomRespectsBaseAndPrecision)
	{
		for(unsigned iteration = 0; iteration < 64; iteration++)
		{
			const TBCD value = TBCD::Random(10, 5, 4);
			EXPECT_EQ(value.Base(), 10U);
			EXPECT_EQ(value.CountInteger(), 5U);
			EXPECT_EQ(value.CountDecimal(), 4U);
			for(const digit_t digit : value.Digits())
				EXPECT_LT(digit, 10U);
		}
		const TBCD bytes = TBCD::Random(0, 16, 0);
		EXPECT_EQ(bytes.Radix(), 256U);
		EXPECT_EQ(bytes.Digits().Count(), 16U);
		EXPECT_TRUE(TBCD::Random(10, 0, 0).IsZero());
		EXPECT_THROW(TBCD::Random(1, 1, 1), TInvalidArgumentException);
	}

	TEST(io_bcd, IntegerArithmeticProperty)
	{
		for(s64_t lhs = -200; lhs <= 200; lhs += 7)
			for(s64_t rhs = -190; rhs <= 190; rhs += 11)
			{
				const TBCD a(lhs, 10, 8, 0);
				const TBCD b(rhs, 10, 8, 0);
				EXPECT_EQ((a + b).ToSignedInt(), lhs + rhs);
				EXPECT_EQ((a - b).ToSignedInt(), lhs - rhs);
				EXPECT_EQ((a * b).ToSignedInt(), lhs * rhs);
				EXPECT_EQ(a.Compare(b), lhs < rhs ? -1 : lhs > rhs ? 1 : 0);
				if(rhs != 0)
				{
					EXPECT_EQ((a / b).ToSignedInt(), lhs / rhs);
					EXPECT_EQ((a % b).ToSignedInt(), lhs % rhs);
				}
			}
	}

	TEST(io_bcd, ArithmeticPropertyAcrossRadices)
	{
		for(const digit_t base : {digit_t(2), digit_t(3), digit_t(10), digit_t(16), digit_t(0)})
		{
			const s64_t scale = (s64_t)(base == 0 ? 256U : (unsigned)base) * (s64_t)(base == 0 ? 256U : (unsigned)base);
			for(s64_t lhs_scaled = -50; lhs_scaled <= 50; lhs_scaled += 7)
				for(s64_t rhs_scaled = -47; rhs_scaled <= 47; rhs_scaled += 9)
				{
					const TBCD lhs = FromScaled(lhs_scaled, base);
					const TBCD rhs = FromScaled(rhs_scaled, base);
					EXPECT_EQ(lhs + rhs, FromScaled(lhs_scaled + rhs_scaled, base));
					EXPECT_EQ(lhs - rhs, FromScaled(lhs_scaled - rhs_scaled, base));
					EXPECT_EQ(lhs * rhs, FromScaled((lhs_scaled * rhs_scaled) / scale, base));

					if(rhs_scaled != 0)
					{
						const s64_t quotient_scaled = (lhs_scaled * scale) / rhs_scaled;
						const s64_t remainder_scaled = (lhs_scaled * scale - quotient_scaled * rhs_scaled) / scale;
						EXPECT_EQ(lhs / rhs, FromScaled(quotient_scaled, base));
						EXPECT_EQ(lhs % rhs, FromScaled(remainder_scaled, base));
					}
				}
		}
	}

	TEST(io_bcd, FixedPointArithmeticProperty)
	{
		for(s64_t lhs_scaled = -250; lhs_scaled <= 250; lhs_scaled += 17)
			for(s64_t rhs_scaled = -240; rhs_scaled <= 240; rhs_scaled += 19)
			{
				TBCD lhs(lhs_scaled < 0 ? -lhs_scaled : lhs_scaled, 10, 4, 2);
				TBCD rhs(rhs_scaled < 0 ? -rhs_scaled : rhs_scaled, 10, 4, 2);
				lhs /= 100;
				rhs /= 100;
				lhs.IsNegative(lhs_scaled < 0 && !lhs.IsZero());
				rhs.IsNegative(rhs_scaled < 0 && !rhs.IsZero());

				const TBCD sum = lhs + rhs;
				const TBCD difference = lhs - rhs;
				const TBCD product = lhs * rhs;
				EXPECT_NEAR(sum.ToDouble(), (lhs_scaled + rhs_scaled) / 100.0, 0.000001);
				EXPECT_NEAR(difference.ToDouble(), (lhs_scaled - rhs_scaled) / 100.0, 0.000001);
				const s64_t product_scaled = (lhs_scaled * rhs_scaled) / 100;
				EXPECT_NEAR(product.ToDouble(), product_scaled / 100.0, 0.000001);

				if(rhs_scaled != 0)
				{
					const TBCD quotient = lhs / rhs;
					const s64_t quotient_scaled = (lhs_scaled * 100) / rhs_scaled;
					EXPECT_NEAR(quotient.ToDouble(), quotient_scaled / 100.0, 0.000001);
				}
			}
	}

	TEST(io_bcd, DigitCountAndBoundsUtilities)
	{
		const TBCD value = Parse("00122.0340");
		EXPECT_EQ(value.CountLeadingZeros(), 2U);
		EXPECT_EQ(value.CountTrailingZeros(), 1U);
		EXPECT_EQ(value.CountSignificantIntegerDigits(), 3U);
		EXPECT_EQ(value.CountSignificantDecimalDigits(), 3U);
		EXPECT_EQ(value.IndexMostSignificantNonZeroDigit(), 2);

		const TBCD rhs = Parse("1.23456");
		EXPECT_EQ(value.OuterBounds(rhs), std::make_tuple(-5, 4));
		EXPECT_EQ(value.InnerBounds(rhs), std::make_tuple(-4, 0));
		EXPECT_THROW(
		{
			const int index = TBCD(0, 10, 2, 2).IndexMostSignificantNonZeroDigit();
			(void)index;
		},
		TInvalidArgumentException
	);
	}

	TEST(io_bcd, ToBCDIntTruncatesFraction)
	{
		const TBCD value = Parse("-123.99");
		const TBCD integer = value.ToBCDInt();
		EXPECT_EQ(integer.CountDecimal(), 0U);
		ExpectValue(integer, "-123");
	}
	TEST(io_bcd, DynamicPrecisionBeyond255Digits)
	{
		TString text;
		for(usys_t i = 0; i < 320; i++)
			text += char32_t((char)('0' + (i % 10)));
		text[0] = '1';

		const TBCD value = TBCD::FromString(text.Reverse(), DECIMAL_SYMBOLS);
		EXPECT_EQ(value.CountInteger(), 320U);
		EXPECT_EQ(value.CountDecimal(), 0U);
		EXPECT_EQ(value.Digits().Count(), 320U);
		EXPECT_EQ(value.Digit(319), 1U);
	}

	TEST(io_bcd, DynamicPrecisionCanBeReconfiguredAndRounded)
	{
		TBCD value = Parse("123.56789");
		EXPECT_FALSE(value.SetPrecision(6, 2, ERoundingMode::TO_NEAREST));
		ExpectValue(value, "123.57");
		EXPECT_EQ(value.CountInteger(), 6U);
		EXPECT_EQ(value.CountDecimal(), 2U);

		EXPECT_TRUE(value.SetPrecision(2, 2, ERoundingMode::TOWARDS_ZERO));
		ExpectValue(value, "23.57");
	}

	TEST(io_bcd, DynamicDivisionIsBoundedByConfiguredPrecision)
	{
		const TBCD one(1, 10, 2, 0);
		const TBCD three(3, 10, 2, 0);
		TBCD quotient(0, 10, 2, 512);
		const TBCD remainder = TBCD::Divide(quotient, one, three);

		EXPECT_EQ(quotient.CountDecimal(), 512U);
		EXPECT_EQ(quotient.Digits().Count(), 514U);
		EXPECT_EQ(quotient.Digit(-1), 3U);
		EXPECT_EQ(quotient.Digit(-512), 3U);
		EXPECT_EQ(remainder.Digit(-512), 1U);
	}

	TEST(io_bcd, DynamicPrecisionUsesU16AndCompactLayout)
	{
		const TBCD maximum(0, 10, MAX_PRECISION, MAX_PRECISION);
		EXPECT_EQ(maximum.CountInteger(), MAX_PRECISION);
		EXPECT_EQ(maximum.CountDecimal(), MAX_PRECISION);
		EXPECT_THROW((TBCD(0, 10, MAX_PRECISION + 1U, 0)), TInvalidArgumentException);
		EXPECT_THROW((TBCD(0, 10, 0, MAX_PRECISION + 1U)), TInvalidArgumentException);

		if(sizeof(void*) == 8U)
		{
			EXPECT_EQ(sizeof(TBCD), 24U);
		}
		EXPECT_EQ(sizeof(TFixedBCD<16, 8, 4, 10>), 17U);
	}

	TEST(io_bcd, SpecialValuesFollowIEEEStyleArithmetic)
	{
		const TBCD zero(0, 10, 8, 4);
		const TBCD one(1, 10, 8, 4);
		const TBCD minus_one(-1, 10, 8, 4);
		const TBCD positive_infinity(std::numeric_limits<double>::infinity(), 10, 8, 4);
		const TBCD negative_infinity(-std::numeric_limits<double>::infinity(), 10, 8, 4);
		const TBCD nan(std::numeric_limits<double>::quiet_NaN(), 10, 8, 4);

		EXPECT_TRUE((positive_infinity + negative_infinity).IsNaN());
		EXPECT_TRUE((positive_infinity - positive_infinity).IsNaN());
		EXPECT_TRUE((positive_infinity * zero).IsNaN());
		EXPECT_TRUE((zero / zero).IsNaN());
		EXPECT_TRUE((one / zero).IsInfinity());
		EXPECT_FALSE((one / zero).IsNegative());
		EXPECT_TRUE((minus_one / zero).IsNegative());
		EXPECT_TRUE((positive_infinity / positive_infinity).IsNaN());
		EXPECT_TRUE((one / positive_infinity).IsZero());
		EXPECT_EQ(one % positive_infinity, one);

		EXPECT_FALSE(nan == nan);
		EXPECT_TRUE(nan != nan);
		EXPECT_FALSE(nan < one);
		EXPECT_FALSE(nan <= one);
		EXPECT_FALSE(nan > one);
		EXPECT_FALSE(nan >= one);
		EXPECT_THROW({ const int unused = nan.Compare(one); (void)unused; }, TInvalidArgumentException);
		EXPECT_LT(negative_infinity, one);
		EXPECT_GT(positive_infinity, one);
		EXPECT_LT(negative_infinity, positive_infinity);

		EXPECT_EQ(TString::Format(U"%d", positive_infinity), "INF");
		EXPECT_EQ(TString::Format(U"%d", negative_infinity), "-INF");
		EXPECT_EQ(TString::Format(U"%d", nan), "NAN");
	}

	TEST(io_bcd, DivisionMarksOnlyNonTerminatingExpansionsPeriodic)
	{
		const TBCD one(1, 10, 4, 0);

		TBCD thirds(0, 10, 4, 6);
		TBCD::Divide(thirds, one, TBCD(3, 10, 4, 0));
		EXPECT_TRUE(thirds.IsPeriodic());
		ExpectValue(thirds, "0.333333");

		TBCD sevenths(0, 10, 4, 2);
		TBCD::Divide(sevenths, one, TBCD(7, 10, 4, 0));
		EXPECT_TRUE(sevenths.IsPeriodic());
		ExpectValue(sevenths, "0.14");

		TBCD eighths(0, 10, 4, 2);
		TBCD::Divide(eighths, one, TBCD(8, 10, 4, 0));
		EXPECT_FALSE(eighths.IsPeriodic());
		ExpectValue(eighths, "0.12");

		TBCD binary_thirds(0, 2, 4, 12);
		TBCD::Divide(binary_thirds, one, TBCD(3, 10, 4, 0));
		EXPECT_TRUE(binary_thirds.IsPeriodic());

		TBCD ternary_thirds(0, 3, 4, 4);
		TBCD::Divide(ternary_thirds, one, TBCD(3, 10, 4, 0));
		EXPECT_FALSE(ternary_thirds.IsPeriodic());
		EXPECT_EQ(ternary_thirds.Digit(-1), 1U);

		TBCD copied = thirds;
		EXPECT_TRUE(copied.IsPeriodic());
		copied += one;
		EXPECT_FALSE(copied.IsPeriodic());
	}

	TEST(io_bcd, FixedBCDStoresDigitsInlineAndUsesCompileTimeSpecs)
	{
		using TFixed = TFixedBCD<12, 6, 2, 10>;
		static_assert(std::is_trivially_destructible_v<TFixed>);
		static_assert(std::is_trivially_copyable_v<TFixed>);
		static_assert(TFixed::CAPACITY == 12);
		static_assert(TFixed::INTEGER_DIGITS == 6);
		static_assert(TFixed::DECIMAL_DIGITS == 2);
		static_assert(TFixed::NUMERIC_BASE == 10);
		static_assert(TFixed::Base() == 10);
		static_assert(TFixed::Radix() == 10);
		static_assert(TFixed::CountInteger() == 6);
		static_assert(TFixed::CountDecimal() == 2);

		const TFixed value(123.45);
		EXPECT_EQ(value.CountInteger(), 6U);
		EXPECT_EQ(value.CountDecimal(), 2U);
		EXPECT_EQ(value.Base(), 10U);
		EXPECT_DOUBLE_EQ(value.ToDouble(), 123.45);
		EXPECT_EQ(sizeof(TFixed), 13U);

		const TFixed zero;
		const u8_t* const raw_zero = reinterpret_cast<const u8_t*>(&zero);
		for(usys_t i = 0; i < sizeof(zero); i++)
			EXPECT_EQ(raw_zero[i], 0U) << "uninitialized/non-zero byte in default TFixedBCD at " << i;

		using TBase256 = TFixedBCD<8, 8, 0, 0>;
		static_assert(TBase256::Radix() == 256U);
		const TBase256 bytes(0x1234U);
		EXPECT_EQ(bytes.Digit(0), 0x34U);
		EXPECT_EQ(bytes.Digit(1), 0x12U);
	}

	TEST(io_bcd, FixedBCDAliasingAndPrimitiveComparison)
	{
		using TFixed = TFixedBCD<32, 20, 2, 10>;
		TFixed value(10.25);
		const TFixed increment(1.75);
		value += increment;
		EXPECT_DOUBLE_EQ(value.ToDouble(), 12.0);
		value -= increment;
		EXPECT_DOUBLE_EQ(value.ToDouble(), 10.25);

		const u64_t large = 9007199254740993ULL;
		const TFixed exact(large);
		EXPECT_EQ(exact, large);
		EXPECT_NE(exact, large - 1U);
		EXPECT_EQ(exact.ToUnsignedInt(), large);
	}

	TEST(io_bcd, FixedBCDConvertsBetweenCompileTimeFormats)
	{
		using TDecimal = TFixedBCD<24, 10, 4, 10>;
		using TBinary = TFixedBCD<24, 12, 8, 2>;
		using THex = TFixedBCD<24, 12, 6, 16>;

		const TDecimal decimal(10.5678);
		using TShortDecimal = TFixedBCD<12, 6, 2, 10>;
		const TShortDecimal short_decimal(decimal);
		EXPECT_NEAR(short_decimal.ToDouble(), 10.56, 0.000001);

		const TBinary binary(decimal);
		EXPECT_NEAR(binary.ToDouble(), 10.56640625, 0.000001);
		EXPECT_EQ(binary.Base(), 2U);
		EXPECT_EQ(binary.CountDecimal(), 8U);

		THex hex;
		hex = binary;
		EXPECT_NEAR(hex.ToDouble(), 10.56640625, 0.000001);
		EXPECT_EQ(hex.Base(), 16U);
		EXPECT_EQ(hex.CountDecimal(), 6U);

		const TBCD dynamic = hex.ToBCD();
		const TDecimal roundtrip(dynamic);
		EXPECT_NEAR(roundtrip.ToDouble(), 10.5664, 0.000001);
	}

	TEST(io_bcd, FixedBCDCrossTypeArithmeticConvertsToResultFormat)
	{
		using TDecimal = TFixedBCD<24, 10, 4, 10>;
		using TBinary = TFixedBCD<24, 12, 8, 2>;
		using THex = TFixedBCD<24, 12, 6, 16>;
		using TQuotient = TFixedBCD<24, 12, 6, 10>;
		const TDecimal decimal(10.5);
		const TBinary binary(2.25);

		THex sum;
		EXPECT_EQ(THex::Add(sum, decimal, binary), 0);
		EXPECT_NEAR(sum.ToDouble(), 12.75, 0.000001);
		EXPECT_GT(decimal, binary);

		THex product;
		THex::Multiply(product, decimal, binary);
		EXPECT_NEAR(product.ToDouble(), 23.625, 0.000001);

		TQuotient quotient;
		const TQuotient remainder = TQuotient::Divide(quotient, decimal, binary);
		EXPECT_NEAR(quotient.ToDouble(), 4.666666, 0.000001);
		EXPECT_LT(std::abs(remainder.ToDouble()), 0.00001);
	}

	TEST(io_bcd, FixedBCDDecimalFloatConversionAndComparison)
	{
		using TFixed = TFixedBCD<12, 6, 6, 10>;
		const TFixed value(114.121);

		// The binary64 value is 114.120999999999995..., so truncation to six
		// decimal digits must preserve the actual IEEE-754 value, not the source
		// code spelling of the literal.
		EXPECT_EQ(value.Digit(2), 1);
		EXPECT_EQ(value.Digit(1), 1);
		EXPECT_EQ(value.Digit(0), 4);
		EXPECT_EQ(value.Digit(-1), 1);
		EXPECT_EQ(value.Digit(-2), 2);
		EXPECT_EQ(value.Digit(-3), 0);
		EXPECT_EQ(value.Digit(-4), 9);
		EXPECT_EQ(value.Digit(-5), 9);
		EXPECT_EQ(value.Digit(-6), 9);
		EXPECT_LT(value, 114.121);
		EXPECT_GT(value, 114.120998);
	}

	TEST(io_bcd, FixedBCDSameBaseArithmeticUsesConfiguredPrecision)
	{
		using TFixed = TFixedBCD<16, 8, 2, 10>;
		using TQuotient = TFixedBCD<16, 8, 3, 10>;
		const TFixed a(Parse("12.34"));
		const TFixed b(Parse("5.67"));

		TFixed sum;
		EXPECT_EQ(TFixed::Add(sum, a, b), 0);
		EXPECT_NEAR(sum.ToDouble(), 18.01, 0.000001);

		TFixed difference;
		EXPECT_EQ(TFixed::Subtract(difference, a, b), 0);
		EXPECT_NEAR(difference.ToDouble(), 6.67, 0.000001);

		TFixed product;
		TFixed::Multiply(product, a, b);
		EXPECT_NEAR(product.ToDouble(), 69.96, 0.000001);

		TQuotient quotient;
		const TQuotient remainder = TQuotient::Divide(quotient, a, b);
		EXPECT_NEAR(quotient.ToDouble(), 2.176, 0.000001);
		EXPECT_NEAR(remainder.ToDouble(), 0.002, 0.000001);
	}

	TEST(io_bcd, FixedBCDSupportsSpecialAndPeriodicValuesWithoutHeapState)
	{
		using TFixed = TFixedBCD<24, 8, 6, 10>;
		using TShort = TFixedBCD<24, 8, 2, 10>;
		using TInt = TFixedBCD<24, 8, 0, 10>;
		const TFixed zero(0);
		const TFixed one(1);
		const TFixed infinity(std::numeric_limits<double>::infinity());
		const TFixed nan(std::numeric_limits<double>::quiet_NaN());

		EXPECT_TRUE(infinity.IsInfinity());
		EXPECT_TRUE(nan.IsNaN());
		EXPECT_FALSE(nan == nan);
		EXPECT_TRUE(nan != nan);
		EXPECT_TRUE((infinity * zero).IsNaN());
		EXPECT_TRUE((one / zero).IsInfinity());

		TFixed thirds;
		TFixed::Divide(thirds, one, TFixed(3));
		EXPECT_TRUE(thirds.IsPeriodic());
		EXPECT_NEAR(thirds.ToDouble(), 0.333333, 0.000001);

		TShort eighths;
		TShort::Divide(eighths, TInt(1), TInt(8));
		EXPECT_FALSE(eighths.IsPeriodic());
		EXPECT_NEAR(eighths.ToDouble(), 0.12, 0.000001);

		const TBCD dynamic_periodic = thirds.ToBCD();
		EXPECT_TRUE(dynamic_periodic.IsPeriodic());
		const TFixed roundtrip(dynamic_periodic);
		EXPECT_TRUE(roundtrip.IsPeriodic());
	}

	TEST(io_bcd, FixedAndDynamicBCDInteroperate)
	{
		using TFixed = TFixedBCD<24, 12, 3, 10>;
		using TBinary = TFixedBCD<24, 20, 4, 2>;
		const TBCD dynamic = Parse("123456.789");
		const TFixed fixed(dynamic);
		EXPECT_EQ(fixed.ToBCD(), dynamic);

		const TBCD binary = TBCD(fixed.ToBCD(), 2, 32, 8);
		const TBinary binary_fixed(binary);
		EXPECT_NEAR(binary_fixed.ToDouble(), 123456.75, 0.000001);
	}


	TEST(io_bcd, DigitViewsCoverAllocatedAndLazyStorage)
	{
		const TBCD zero(0, 10, 3, 2);
		EXPECT_EQ(zero.Digits().Count(), 0U);
		EXPECT_EQ(zero.IntegerDigits().Count(), 0U);
		EXPECT_EQ(zero.DecimalDigits().Count(), 0U);
		EXPECT_EQ(zero.CountLeadingZeros(), 3U);
		EXPECT_EQ(zero.CountTrailingZeros(), 2U);

		const TBCD value = Parse("123.45");
		const auto digits = value.Digits();
		const auto integers = value.IntegerDigits();
		const auto decimals = value.DecimalDigits();
		ASSERT_EQ(digits.Count(), 5U);
		ASSERT_EQ(integers.Count(), 3U);
		ASSERT_EQ(decimals.Count(), 2U);
		EXPECT_EQ(digits[0], 5U);
		EXPECT_EQ(digits[1], 4U);
		EXPECT_EQ(integers[0], 3U);
		EXPECT_EQ(integers[1], 2U);
		EXPECT_EQ(integers[2], 1U);
		EXPECT_EQ(decimals[0], 5U);
		EXPECT_EQ(decimals[1], 4U);
	}

	TEST(io_bcd, FloatingComparisonCoversZeroSignsAndSpecialValues)
	{
		const double inf = std::numeric_limits<double>::infinity();
		const double nan = std::numeric_limits<double>::quiet_NaN();
		const TBCD zero(0, 10, 4, 4);
		const TBCD positive = Parse("1.5");
		const TBCD negative = Parse("-1.5");
		const TBCD positive_infinity(inf, 10, 4, 4);
		const TBCD negative_infinity(-inf, 10, 4, 4);

		EXPECT_EQ(zero, 0.0);
		EXPECT_EQ(zero, -0.0);
		EXPECT_GT(positive, -1.0);
		EXPECT_LT(negative, 1.0);
		EXPECT_LT(negative, -1.0);
		EXPECT_GT(negative, -2.0);

		EXPECT_EQ(positive_infinity, inf);
		EXPECT_EQ(negative_infinity, -inf);
		EXPECT_GT(positive_infinity, -inf);
		EXPECT_LT(negative_infinity, inf);
		EXPECT_GT(positive_infinity, 1.0);
		EXPECT_LT(negative_infinity, 1.0);
		EXPECT_LT(positive, inf);
		EXPECT_GT(positive, -inf);

		EXPECT_FALSE(positive == nan);
		EXPECT_TRUE(positive != nan);
		EXPECT_FALSE(positive < nan);
		EXPECT_FALSE(positive <= nan);
		EXPECT_FALSE(positive > nan);
		EXPECT_FALSE(positive >= nan);
	}

	TEST(io_bcd, SpecialValueArithmeticCoversPropagationAndReusableOutput)
	{
		const double inf = std::numeric_limits<double>::infinity();
		const TBCD zero(0, 10, 8, 4);
		const TBCD one(1, 10, 8, 4);
		const TBCD two(2, 10, 8, 4);
		const TBCD three(3, 10, 8, 4);
		const TBCD minus_one(-1, 10, 8, 4);
		const TBCD positive_infinity(inf, 10, 8, 4);
		const TBCD negative_infinity(-inf, 10, 8, 4);
		const TBCD nan(std::numeric_limits<double>::quiet_NaN(), 10, 8, 4);
		TBCD out(0, 10, 8, 4);

		EXPECT_EQ(TBCD::Add(out, nan, one), 0);
		EXPECT_TRUE(out.IsNaN());
		EXPECT_EQ(TBCD::Add(out, positive_infinity, one), 0);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_FALSE(out.IsNegative());
		EXPECT_EQ(TBCD::Add(out, one, negative_infinity), 0);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_TRUE(out.IsNegative());
		EXPECT_EQ(TBCD::Add(out, positive_infinity, positive_infinity), 0);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_FALSE(out.IsNegative());

		EXPECT_EQ(TBCD::Subtract(out, nan, one), 0);
		EXPECT_TRUE(out.IsNaN());
		EXPECT_EQ(TBCD::Subtract(out, positive_infinity, one), 0);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_FALSE(out.IsNegative());
		EXPECT_EQ(TBCD::Subtract(out, one, positive_infinity), 0);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_TRUE(out.IsNegative());
		EXPECT_EQ(TBCD::Subtract(out, positive_infinity, negative_infinity), 0);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_FALSE(out.IsNegative());

		TBCD::Multiply(out, positive_infinity, minus_one);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_TRUE(out.IsNegative());
		TBCD::Multiply(out, one, negative_infinity);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_TRUE(out.IsNegative());
		TBCD::Multiply(out, zero, positive_infinity);
		EXPECT_TRUE(out.IsNaN());

		TBCD remainder = TBCD::Divide(out, nan, one);
		EXPECT_TRUE(out.IsNaN());
		EXPECT_TRUE(remainder.IsNaN());
		remainder = TBCD::Divide(out, positive_infinity, minus_one);
		EXPECT_TRUE(out.IsInfinity());
		EXPECT_TRUE(out.IsNegative());
		EXPECT_TRUE(remainder.IsNaN());

		out.SetInfinity();
		EXPECT_EQ(TBCD::Add(out, one, two), 0);
		EXPECT_EQ(out, 3);
		out.SetNaN();
		EXPECT_EQ(TBCD::Subtract(out, three, one), 0);
		EXPECT_EQ(out, 2);
		out.SetInfinity();
		TBCD::Multiply(out, two, three);
		EXPECT_EQ(out, 6);
		out.SetNaN();
		remainder = TBCD::Divide(out, TBCD(6, out), TBCD(3, out));
		EXPECT_EQ(out, 2);
		EXPECT_TRUE(remainder.IsZero());
	}

	TEST(io_bcd, AbsoluteComparisonCoversInfinityCases)
	{
		const double inf = std::numeric_limits<double>::infinity();
		const TBCD positive_infinity(inf, 10, 2, 0);
		const TBCD negative_infinity(-inf, 10, 2, 0);
		const TBCD one(1, 10, 2, 0);

		EXPECT_EQ(positive_infinity.Compare(negative_infinity, true), 0);
		EXPECT_EQ(positive_infinity.Compare(positive_infinity), 0);
		EXPECT_GT(positive_infinity.Compare(negative_infinity), 0);
		EXPECT_GT(positive_infinity.Compare(one, true), 0);
		EXPECT_LT(one.Compare(positive_infinity, true), 0);
		EXPECT_GT(one.Compare(negative_infinity), 0);
	}

	TEST(io_bcd, AddSubtractDetectDiscardedHighDigits)
	{
		TBCD out(0, 10, 2, 0);
		const TBCD hundred(100, 10, 3, 0);
		const TBCD zero(0, 10, 3, 0);

		EXPECT_NE(TBCD::Add(out, hundred, zero), 0);
		EXPECT_TRUE(out.IsZero());
		EXPECT_NE(TBCD::Subtract(out, hundred, zero), 0);
		EXPECT_TRUE(out.IsZero());

		TBCD binary_out(0, 2, 2, 0);
		EXPECT_NE(TBCD::Add(binary_out, hundred, zero), 0);
	}

	TEST(io_bcd, ShiftCoversNoopSpecialAndBoundaryCases)
	{
		const TBCD original = Parse("012.34");
		TBCD value(original);
		value <<= 0;
		EXPECT_EQ(value, original);
		value >>= 0;
		EXPECT_EQ(value, original);

		TBCD zero(0, 10, 3, 2);
		zero <<= 1;
		EXPECT_TRUE(zero.IsZero());
		zero >>= 1;
		EXPECT_TRUE(zero.IsZero());

		TBCD right = original;
		right >>= 5;
		EXPECT_TRUE(right.IsZero());

		TBCD infinity(std::numeric_limits<double>::infinity(), 10, 3, 2);
		infinity <<= 1;
		EXPECT_TRUE(infinity.IsInfinity());
		infinity >>= 1;
		EXPECT_TRUE(infinity.IsInfinity());
		infinity.Shift(1);
		EXPECT_TRUE(infinity.IsInfinity());

		EXPECT_EQ(original << 1, Parse("123.40"));
		EXPECT_EQ(original >> 1, Parse("001.23"));
	}

	TEST(io_bcd, RoundCoversNoopLowerTieDigitsAndBase256Stochastic)
	{
		TBCD infinity(std::numeric_limits<double>::infinity(), 10, 2, 3);
		infinity.Round(1, ERoundingMode::TO_NEAREST);
		EXPECT_TRUE(infinity.IsInfinity());

		TBCD unchanged = Parse("1.234");
		const TBCD unchanged_copy = unchanged;
		unchanged.Round(3, ERoundingMode::TO_NEAREST);
		EXPECT_EQ(unchanged, unchanged_copy);

		TBCD zero(0, 10, 2, 3);
		zero.Round(1, ERoundingMode::TO_NEAREST);
		EXPECT_TRUE(zero.IsZero());

		TBCD exact = Parse("1.200");
		exact.Round(1, ERoundingMode::TO_NEAREST_EVEN);
		ExpectValue(exact, "1.200");

		TBCD above_tie = Parse("1.251");
		above_tie.Round(1, ERoundingMode::TO_NEAREST_EVEN);
		ExpectValue(above_tie, "1.300");

		for(unsigned i = 0; i < 4; i++)
		{
			TBCD base256(0, 0, 2, 2);
			base256.Digit(0, 1);
			base256.Digit(-2, 128);
			base256.Round(1, ERoundingMode::STOCHASTIC);
			EXPECT_EQ(base256.Digit(-2), 0U);
			EXPECT_LE(base256.Digit(-1), 1U);
			EXPECT_EQ(base256.Digit(0), 1U);
		}
	}

	TEST(io_bcd, SetPrecisionCoversNoopSpecialAndZeroSizedResults)
	{
		TBCD value = Parse("123.45");
		EXPECT_FALSE(value.SetPrecision(value.CountInteger(), value.CountDecimal()));

		TBCD infinity(std::numeric_limits<double>::infinity(), 10, 2, 2);
		EXPECT_FALSE(infinity.SetPrecision(7, 3));
		EXPECT_TRUE(infinity.IsInfinity());
		EXPECT_EQ(infinity.CountInteger(), 7U);
		EXPECT_EQ(infinity.CountDecimal(), 3U);

		TBCD truncated = Parse("123.45");
		EXPECT_TRUE(truncated.SetPrecision(2, 2, ERoundingMode::TOWARDS_ZERO));
		ExpectValue(truncated, "23.45");

		TBCD zero = Parse("0.00");
		EXPECT_FALSE(zero.SetPrecision(0, 0));
		EXPECT_TRUE(zero.IsZero());
		EXPECT_EQ(zero.Digits().Count(), 0U);
	}

	TEST(io_bcd, NarrowIntegerConstructorsConfigurationReferencesAndCasts)
	{
		const TBCD u8_value((u8_t)250, 10, 3, 0);
		const TBCD s8_value((s8_t)-120, 10, 3, 0);
		const TBCD u16_value((u16_t)65000, 10, 5, 0);
		const TBCD s16_value((s16_t)-32000, 10, 5, 0);
		const TBCD u32_value((u32_t)4000000000U, 10, 10, 0);
		const TBCD s32_value((s32_t)-2000000000, 10, 10, 0);
		EXPECT_EQ(u8_value, 250);
		EXPECT_EQ(s8_value, -120);
		EXPECT_EQ(u16_value, 65000);
		EXPECT_EQ(s16_value, -32000);
		EXPECT_EQ(u32_value, (u64_t)4000000000U);
		EXPECT_EQ(s32_value, (s64_t)-2000000000LL);

		const TBCD conf(0, 10, 12, 3);
		EXPECT_EQ(TBCD((u8_t)42, conf), 42);
		EXPECT_EQ(TBCD((s8_t)-42, conf), -42);
		EXPECT_EQ(TBCD((u16_t)1234, conf), 1234);
		EXPECT_EQ(TBCD((s16_t)-1234, conf), -1234);
		EXPECT_EQ(TBCD((u32_t)123456, conf), 123456);
		EXPECT_EQ(TBCD((float)1.25, conf), 1.25);

		TBCD assigned(0, 10, 12, 3);
		assigned = (s64_t)-123456789;
		EXPECT_EQ(assigned, (s64_t)-123456789);
		TBCD* const self = &assigned;
		assigned = std::move(*self);
		EXPECT_EQ(assigned, (s64_t)-123456789);

		const TBCD cast_source = Parse("123.00");
		EXPECT_DOUBLE_EQ((double)cast_source, 123.0);
		EXPECT_EQ((s64_t)cast_source, (s64_t)123);
		EXPECT_EQ((u64_t)cast_source, (u64_t)123);
	}

	TEST(io_bcd, PrimitiveArithmeticWrappers)
	{
		const TBCD base = Parse("08.00");
		ExpectValue(base + 2.0, "10.00");
		ExpectValue(base - 2.0, "6.00");
		ExpectValue(base * 2.0, "16.00");
		ExpectValue(base / 2.0, "4.00");
		ExpectValue(base % 3.0, "0.02");

		ExpectValue(base + (u64_t)2, "10.00");
		ExpectValue(base - (u64_t)2, "6.00");
		ExpectValue(base * (u64_t)2, "16.00");
		ExpectValue(base / (u64_t)2, "4.00");
		ExpectValue(base % (u64_t)3, "0.02");

		ExpectValue(base + (s64_t)-2, "6.00");
		ExpectValue(base - (s64_t)-2, "10.00");
		ExpectValue(base * (s64_t)-2, "-16.00");
		ExpectValue(base / (s64_t)-2, "-4.00");
		ExpectValue(base % (s64_t)3, "0.02");

		ExpectValue(base + 2, "10.00");
		ExpectValue(base - 2, "6.00");
		ExpectValue(base * 2, "16.00");
		ExpectValue(base / 2, "4.00");
		ExpectValue(base % 3, "0.02");

		TBCD value = base;
		value += 2.0;
		value -= 2.0;
		value *= 2.0;
		value /= 2.0;
		value %= 3.0;
		ExpectValue(value, "0.02");

		value = base;
		value += (u64_t)2;
		value -= (u64_t)2;
		value *= (u64_t)2;
		value /= (u64_t)2;
		value %= (u64_t)3;
		ExpectValue(value, "0.02");

		value = base;
		value += (s64_t)2;
		value -= (s64_t)2;
		value *= (s64_t)2;
		value /= (s64_t)2;
		value %= (s64_t)3;
		ExpectValue(value, "0.02");

		value = base;
		value += 2;
		value -= 2;
		value *= 2;
		value /= 2;
		value %= 3;
		ExpectValue(value, "0.02");
	}

	TEST(io_bcd, PrimitiveComparisonsCoverAllIntegralWrappers)
	{
		const TBCD value = Parse("5.00");
		EXPECT_EQ(value, (u64_t)5);
		EXPECT_NE(value, (u64_t)4);
		EXPECT_GE(value, (u64_t)5);
		EXPECT_LE(value, (u64_t)5);
		EXPECT_GT(value, (u64_t)4);
		EXPECT_LT(value, (u64_t)6);

		EXPECT_EQ(value, (s64_t)5);
		EXPECT_NE(value, (s64_t)-5);
		EXPECT_GE(value, (s64_t)5);
		EXPECT_LE(value, (s64_t)5);
		EXPECT_GT(value, (s64_t)-5);
		EXPECT_LT(value, (s64_t)6);

		EXPECT_EQ(value, 5);
		EXPECT_NE(value, 4);
		EXPECT_GE(value, 5);
		EXPECT_LE(value, 5);
		EXPECT_GT(value, 4);
		EXPECT_LT(value, 6);
	}

	TEST(io_bcd, BCDConversionCoversSpecialValuesAndPeriodicPrecisionChange)
	{
		const TBCD positive_infinity(std::numeric_limits<double>::infinity(), 10, 4, 2);
		const TBCD nan(std::numeric_limits<double>::quiet_NaN(), 10, 4, 2);
		const TBCD binary_infinity(positive_infinity, 2, 8, 8);
		const TBCD binary_nan(nan, 2, 8, 8);
		EXPECT_TRUE(binary_infinity.IsInfinity());
		EXPECT_FALSE(binary_infinity.IsNegative());
		EXPECT_TRUE(binary_nan.IsNaN());

		TBCD periodic(0, 10, 4, 6);
		TBCD::Divide(periodic, TBCD(1, 10, 4, 0), TBCD(3, 10, 4, 0));
		ASSERT_TRUE(periodic.IsPeriodic());
		TBCD shorter(0, 10, 4, 2);
		shorter = periodic;
		EXPECT_FALSE(shorter.IsPeriodic());
		ExpectValue(shorter, "0.33");
	}

	TEST(io_bcd, RandomCoversBase256AndEmptyConfiguration)
	{
		const TBCD empty = TBCD::Random(10, 0, 0);
		EXPECT_TRUE(empty.IsZero());
		EXPECT_EQ(empty.Digits().Count(), 0U);

		const TBCD random = TBCD::Random(0, 8, 8);
		EXPECT_EQ(random.Base(), 0U);
		EXPECT_EQ(random.Radix(), 256U);
		EXPECT_EQ(random.Digits().Count(), 16U);
	}

	TEST(io_bcd, RequiredDigitsSameBaseReturnsSourceCount)
	{
		EXPECT_EQ(TBCD::RequiredDigits(10, 10, 1234), 1234U);
		EXPECT_EQ(TBCD::RequiredDigits(0, 0, 1234), 1234U);
	}

	TEST(io_bcd, InvalidSettersRemainSafe)
	{
		TBCD invalid;
		EXPECT_TRUE(invalid.IsInvalid());
		invalid.SetZero();
		EXPECT_TRUE(invalid.IsInvalid());
		invalid.SetNaN();
		EXPECT_TRUE(invalid.IsInvalid());
		invalid.SetInfinity();
		EXPECT_TRUE(invalid.IsInvalid());
	}

	TEST(io_bcd, ProtectedDefensivePathsRemainCorrect)
	{
		TBCD out(7, 10, 2, 0);
		const TBCD zero(0, 10, 2, 0);
		const TBCD one(1, 10, 2, 0);
		TBCDTestAccess::AbsMul(out, zero, one);
		EXPECT_TRUE(out.IsZero());

		TBCDTestAccess self(42, 10, 2, 0);
		self.ConvertBCD(self);
		EXPECT_EQ(self, 42);

		const TBCD infinity(std::numeric_limits<double>::infinity(), 10, 2, 0);
		EXPECT_FALSE(infinity.IsZero());
	}

}
