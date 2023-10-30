#include <gtest/gtest.h>
#include <math.h>
#include <el1/error.hpp>
#include <el1/io_types.hpp>
#include <el1/io_bcd.hpp>
#include <el1/system_random.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::io::types;
	using namespace el1::io::bcd;
	using namespace el1::math;
	using namespace el1::system::random;

	TEST(io_bcd, TBCD_construct_from_integer)
	{
		{
			TBCD v(0, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_TRUE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 0);
			EXPECT_EQ(v.Digit(1), 0);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((s8_t )10, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 0);
			EXPECT_EQ(v.Digit(1), 1);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((u8_t )21, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 1);
			EXPECT_EQ(v.Digit(1), 2);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((s16_t)32767, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 7);
			EXPECT_EQ(v.Digit(1), 6);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((u16_t)0xabcd, 16, 5, 0);
			EXPECT_EQ(v.Base(), 16U);
			EXPECT_EQ(v.CountInteger(), 5U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 0xd);
			EXPECT_EQ(v.Digit(1), 0xc);
			EXPECT_EQ(v.Digit(2), 0xb);
			EXPECT_EQ(v.Digit(3), 0xa);
			EXPECT_EQ(v.Digit(4), 0);
			EXPECT_EQ(v.Digit(5), 0);
		}

		{
			TBCD v((s16_t)32767, 10, 5, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 5U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 7);
			EXPECT_EQ(v.Digit(1), 6);
			EXPECT_EQ(v.Digit(2), 7);
			EXPECT_EQ(v.Digit(3), 2);
			EXPECT_EQ(v.Digit(4), 3);
			EXPECT_EQ(v.Digit(5), 0);
		}

		{
			TBCD v((u16_t)99, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 9);
			EXPECT_EQ(v.Digit(1), 9);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((s32_t)3, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 3);
			EXPECT_EQ(v.Digit(1), 0);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((u32_t)143, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 3);
			EXPECT_EQ(v.Digit(1), 4);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((u32_t)143, 10, 2, 7);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 7U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 3);
			EXPECT_EQ(v.Digit(1), 4);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((s64_t)-15, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_TRUE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 5);
			EXPECT_EQ(v.Digit(1), 1);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v((u64_t)10, 10, 2, 0);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 0U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-1), 0);
			EXPECT_EQ(v.Digit(0), 0);
			EXPECT_EQ(v.Digit(1), 1);
			EXPECT_EQ(v.Digit(2), 0);
		}
	}

	TEST(io_bcd, TBCD_construct_from_float)
	{
		{
			TBCD v(10.751, 10, 2, 3);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 3U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-4), 0);
			EXPECT_EQ(v.Digit(-3), 1);
			EXPECT_EQ(v.Digit(-2), 5);
			EXPECT_EQ(v.Digit(-1), 7);
			EXPECT_EQ(v.Digit(0), 0);
			EXPECT_EQ(v.Digit(1), 1);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v(-10.751, 10, 2, 3);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 3U);
			EXPECT_TRUE(v.IsNegative());
			EXPECT_FALSE(v.IsZero());
			EXPECT_EQ(v.Digit(-4), 0);
			EXPECT_EQ(v.Digit(-3), 1);
			EXPECT_EQ(v.Digit(-2), 5);
			EXPECT_EQ(v.Digit(-1), 7);
			EXPECT_EQ(v.Digit(0), 0);
			EXPECT_EQ(v.Digit(1), 1);
			EXPECT_EQ(v.Digit(2), 0);
		}

		{
			TBCD v(0.0, 10, 2, 3);
			EXPECT_EQ(v.Base(), 10U);
			EXPECT_EQ(v.CountInteger(), 2U);
			EXPECT_EQ(v.CountDecimal(), 3U);
			EXPECT_FALSE(v.IsNegative());
			EXPECT_TRUE(v.IsZero());
		}
	}

	TEST(io_bcd, TBCD_Compare)
	{
		EXPECT_LT(TBCD(122, 10, 8, 0), TBCD(277, 10, 8, 0));
		EXPECT_GT(TBCD(277, 10, 8, 0), TBCD(122, 10, 8, 0));
		EXPECT_LT(TBCD(-277, 10, 8, 0), TBCD(122, 10, 8, 0));
		EXPECT_GT(TBCD(-277, 10, 8, 0).Compare(TBCD(122, 10, 8, 0), true), 0);
		EXPECT_GT(TBCD(277, 10, 8, 0).Compare(TBCD(122, 10, 8, 0), true), 0);
		EXPECT_LT(TBCD(-122, 10, 8, 0).Compare(TBCD(277, 10, 8, 0), false), 0);
		EXPECT_EQ(TBCD(-122, 10, 8, 0).Compare(TBCD(-122, 10, 8, 0), false), 0);
		EXPECT_EQ(TBCD(122, 10, 8, 0).Compare(TBCD(122, 10, 8, 0), false), 0);
		EXPECT_EQ(TBCD(0, 10, 8, 0).Compare(TBCD(0, 10, 8, 0), false), 0);
		EXPECT_EQ(TBCD(0, 10, 8, 0).Compare(TBCD(0, 10, 8, 0), true), 0);

		TXorShift rng;
		for(unsigned i = 0; i < 1000; i++)
		{
			const double xd = round(rng.Float(true) * 100.0 * 100.0) / 100.0;
			const double yd = round(rng.Float(true) * 100.0 * 100.0) / 100.0;
			const TBCD x(xd, 10, 4, 4);
			const TBCD y(yd, 10, 4, 4);
			EXPECT_EQ(x > y, xd > yd);
			EXPECT_EQ(x < y, xd < yd);
			EXPECT_EQ(x >= y, xd >= yd);
			EXPECT_EQ(x <= y, xd <= yd);
			EXPECT_EQ(x == y, xd == yd);
			EXPECT_EQ(x != y, xd != yd);

			if(HasFailure())
			{
				std::cerr<<"operands were: x = "<<xd<<", y = "<<yd<<"\n\n";
				break;
			}
		}
	}

	TEST(io_bcd, TBCD_Add_Sub)
	{
		{
			TBCD a(122, 10, 8, 0);
			TBCD b(277, 10, 8, 0);
			EXPECT_EQ(a, 122);
			EXPECT_EQ(b, 277);
			TBCD c = a + b;
			EXPECT_EQ(c, 399);
		}

		{
			TBCD a(122, 2, 10, 3);
			TBCD b(277, 2, 10, 3);
			EXPECT_EQ(a, 122);
			EXPECT_EQ(b, 277);
			TBCD c = a + b;
			EXPECT_EQ(c, 399);
		}

		{
			TBCD a(122, 2, 10, 1);
			TBCD b(-277, 2, 10, 1);
			EXPECT_EQ(a, 122);
			EXPECT_EQ(b, -277);
			TBCD c = a + b;
			EXPECT_EQ(c, -155);
		}

		{
			TBCD a(-122, 3, 10, 1);
			TBCD b(277, 3, 10, 1);
			EXPECT_EQ(a, -122);
			EXPECT_EQ(b, 277);
			TBCD c = a + b;
			EXPECT_EQ(c, 155);
		}

		{
			TBCD a(122, 10, 8, 0);
			TBCD b(277, 10, 8, 0);
			EXPECT_EQ(a, 122);
			EXPECT_EQ(b, 277);
			TBCD c = a - b;
			EXPECT_EQ(c, -155);
		}

		{
			TBCD a(122, 2, 10, 3);
			TBCD b(277, 2, 10, 3);
			EXPECT_EQ(a, 122);
			EXPECT_EQ(b, 277);
			TBCD c = a - b;
			EXPECT_EQ(c, -155);
		}

		{
			TBCD a(122, 2, 10, 1);
			TBCD b(-277, 2, 10, 1);
			EXPECT_EQ(a, 122);
			EXPECT_EQ(b, -277);
			TBCD c = a - b;
			EXPECT_EQ(c, 399);
		}

		{
			TBCD a(-122, 3, 10, 1);
			TBCD b(277, 3, 10, 1);
			EXPECT_EQ(a, -122);
			EXPECT_EQ(b, 277);
			TBCD c = a - b;
			EXPECT_EQ(c, -399);
		}

		{
			TBCD a(-0.7, 5, 1, 5);
			TBCD b( 0.7, 5, 1, 5);
			EXPECT_EQ(a, -0.7);
			EXPECT_EQ(b,  0.7);
			TBCD c = a + b;
			EXPECT_EQ(c, 0);
		}
	}

	TEST(io_bcd, TBCD_CountXDigits)
	{
		{
			TBCD v(0, 10, 8, 2);
			EXPECT_EQ(v.CountLeadingZeros(),  8U);
			EXPECT_EQ(v.CountTrailingZeros(), 2U);
			EXPECT_EQ(v.CountSignificantIntegerDigits(), 0U);
			EXPECT_EQ(v.CountSignificantDecimalDigits(), 0U);
		}

		{
			TBCD v(-122, 10, 8, 2);
			EXPECT_EQ(v.CountLeadingZeros(),  5U);
			EXPECT_EQ(v.CountTrailingZeros(), 2U);
			EXPECT_EQ(v.CountSignificantIntegerDigits(), 3U);
			EXPECT_EQ(v.CountSignificantDecimalDigits(), 0U);
		}

		{
			TBCD v(122, 10, 8, 2);
			EXPECT_EQ(v.CountLeadingZeros(),  5U);
			EXPECT_EQ(v.CountTrailingZeros(), 2U);
			EXPECT_EQ(v.CountSignificantIntegerDigits(), 3U);
			EXPECT_EQ(v.CountSignificantDecimalDigits(), 0U);
		}

		{
			TBCD v(122, 10, 3, 2);
			EXPECT_EQ(v.CountLeadingZeros(),  0U);
			EXPECT_EQ(v.CountTrailingZeros(), 2U);
			EXPECT_EQ(v.CountSignificantIntegerDigits(), 3U);
			EXPECT_EQ(v.CountSignificantDecimalDigits(), 0U);
		}

		{
			TBCD v(17.2575, 10, 3, 5);
			EXPECT_EQ(v.CountLeadingZeros(),  1U);
			EXPECT_EQ(v.CountTrailingZeros(), 1U);
			EXPECT_EQ(v.CountSignificantIntegerDigits(), 2U);
			EXPECT_EQ(v.CountSignificantDecimalDigits(), 4U);
		}
	}

	TEST(io_bcd, TBCD_assignment)
	{
		{
			TBCD a(122, 10, 8, 2);
			EXPECT_EQ(a.Base(), 10U);
			EXPECT_EQ(a.CountInteger(), 8U);
			EXPECT_EQ(a.CountDecimal(), 2U);

			a = -177;
			EXPECT_EQ(a, -177);
			EXPECT_EQ(a.Base(), 10U);
			EXPECT_EQ(a.CountInteger(), 8U);
			EXPECT_EQ(a.CountDecimal(), 2U);

			a = 133;
			EXPECT_EQ(a, 133);
			EXPECT_EQ(a.Base(), 10U);
			EXPECT_EQ(a.CountInteger(), 8U);
			EXPECT_EQ(a.CountDecimal(), 2U);

			// a = TBCD(12, 3, 2, 1);
			// EXPECT_EQ(a, 12);
			// EXPECT_EQ(a.Base(), 10U);
			// EXPECT_EQ(a.CountInteger(), 8U);
			// EXPECT_EQ(a.CountDecimal(), 2U);

			a = 17.356;
			EXPECT_EQ(a, 17.356);
			EXPECT_EQ(a.Base(), 10U);
			EXPECT_EQ(a.CountInteger(), 8U);
			EXPECT_EQ(a.CountDecimal(), 2U);

			a = 0;
			EXPECT_EQ(a, 0);
			EXPECT_EQ(a.Base(), 10U);
			EXPECT_EQ(a.CountInteger(), 8U);
			EXPECT_EQ(a.CountDecimal(), 2U);

			a = (s64_t)-33;
			EXPECT_EQ(a, -33);
			EXPECT_EQ(a.Base(), 10U);
			EXPECT_EQ(a.CountInteger(), 8U);
			EXPECT_EQ(a.CountDecimal(), 2U);

			a = (u64_t)33;
			EXPECT_EQ(a, 33);
			EXPECT_EQ(a.Base(), 10U);
			EXPECT_EQ(a.CountInteger(), 8U);
			EXPECT_EQ(a.CountDecimal(), 2U);
		}
	}

	TEST(io_bcd, TBCD_Round)
	{
		{
			TBCD a(22.123456789, 10, 2, 10);
			a.Round(2, ERoundingMode::TO_NEAREST);
			EXPECT_EQ(a, 22.12);
		}

		{
			TBCD a(22.123456789, 10, 2, 10);
			a.Round(3, ERoundingMode::TO_NEAREST);
			EXPECT_EQ(a, 22.123);
		}

		{
			TBCD a(22.123456789, 10, 2, 10);
			a.Round(4, ERoundingMode::TO_NEAREST);
			EXPECT_EQ(a, 22.1235);
		}

		{
			TBCD a(22.123456789, 10, 2, 10);
			a.Round(5, ERoundingMode::TO_NEAREST);
			EXPECT_EQ(a, 22.12346);
		}

		{
			TBCD a(22.123456789, 10, 2, 10);
			a.Round(0, ERoundingMode::UPWARD);
			EXPECT_EQ(a, 23.0);
		}

		{
			TBCD a(22.123456789, 10, 2, 10);
			a.Round(0, ERoundingMode::DOWNWARD);
			EXPECT_EQ(a, 22.0);
		}

		{
			TBCD a(22.999, 10, 2, 10);
			a.Round(0, ERoundingMode::DOWNWARD);
			EXPECT_EQ(a, 22.0);
		}
	}

	static bool CompareEpsilon(const double lhs, const double rhs, const double epsilon = 0.00000000000001)
	{
		if(lhs + epsilon >= rhs && lhs - epsilon <= rhs)
			return true;
		fprintf(stderr, "lhs = %0.20f\nrhs = %0.20f\n", lhs, rhs);
		return false;
	}

	TEST(io_bcd, TBCD_ToIntegerDouble)
	{
		{
			const double ref = 22.999;
			TBCD a(ref, 10, 10, 20);
			EXPECT_EQ(a.ToUnsignedInt(), 22U);
			EXPECT_EQ(a.ToSignedInt(), 22);
			EXPECT_TRUE(CompareEpsilon(a.ToDouble(), ref));
		}

		{
			const double ref = -22.999;
			TBCD a(ref, 10, 10, 20);
			EXPECT_EQ(a.ToUnsignedInt(), 22U);
			EXPECT_EQ(a.ToSignedInt(), -22);
			EXPECT_TRUE(CompareEpsilon(a.ToDouble(), ref));
		}

		{
			const double ref = 18446744073709551615.0;
			TBCD a(18446744073709551615ULL, 33, 20, 4);
			EXPECT_EQ(a.ToUnsignedInt(), 18446744073709551615ULL);
			EXPECT_EQ(a.ToSignedInt(), -1LL);
			EXPECT_TRUE(CompareEpsilon(a.ToDouble(), ref));
		}

		{
			const double ref = -9223372036854775807.0;
			TBCD a(-9223372036854775807LL, 31, 20, 4);
			EXPECT_EQ(a.ToUnsignedInt(), 9223372036854775807ULL);
			EXPECT_EQ(a.ToSignedInt(), -9223372036854775807LL);
			EXPECT_TRUE(CompareEpsilon(a.ToDouble(), ref));
		}
	}

	TEST(io_bcd, TBCD_IsZero)
	{
		{
			TBCD v(0, 10, 10, 10);
			EXPECT_TRUE(v.IsZero());
			v += 1;
			EXPECT_FALSE(v.IsZero());
			v -= 1;
			EXPECT_TRUE(v.IsZero());
		}

		{
			TBCD v(1, 10, 10, 10);
			EXPECT_FALSE(v.IsZero());
			v -= 1;
			EXPECT_TRUE(v.IsZero());
		}

		{
			TBCD v(0, 10, 10, 10);
			EXPECT_TRUE(v.IsZero());
			v.Digit(0, 1);
			EXPECT_FALSE(v.IsZero());
			v -= 1;
			EXPECT_TRUE(v.IsZero());
		}
	}

	TEST(io_bcd, TBCD_Multiply)
	{
		{
			TBCD a(12, 10, 8, 0);
			TBCD b(13, 10, 8, 0);
			TBCD c = a * b;
			EXPECT_EQ(c, 156);
		}

		{
			TBCD a( 1111, 10, 8, 0);
			TBCD b(-2222, 10, 8, 0);
			TBCD c = a * b;
			EXPECT_EQ(c, -2468642);
		}

		{
			TBCD a(-1234, 10, 8, 0);
			TBCD b(-5678, 10, 8, 0);
			TBCD c = a * b;
			EXPECT_EQ(c, 7006652);
		}

		{
			TBCD a(-1234.17, 10, 8, 2);
			TBCD b( 5678.35, 10, 8, 2);
			TBCD c = a * b;
			EXPECT_EQ(c, -7008049.2195);
		}

		{
			TBCD a(35, 10, 8, 1);
			TBCD b( 0, 10, 8, 1);
			b.Digit(-1, 7);
			TBCD c = a * b;
			EXPECT_EQ(c, 24.5);
		}
	}

	TEST(io_bcd, TBCD_Divide)
	{
		{
			TBCD a(10, 10, 8, 0);
			TBCD b( 1, 10, 8, 0);
			TBCD c = a / b;
			EXPECT_EQ(c, 10);
		}

		{
			TBCD a(35, 10, 8, 0);
			TBCD b(10, 10, 8, 0);
			TBCD c = a / b;
			EXPECT_EQ(c, 3.5);
		}

		{
			TBCD a(35, 10, 8, 1);
			TBCD b( 0, 10, 8, 1);
			b.Digit(-1, 7);
			TBCD c = a / b;
			EXPECT_EQ(c, 50);
		}
	}

	TEST(io_bcd, TBCD_Add_loop)
	{
		TXorShift rng;

		for(unsigned i = 0; i < 1000; i++)
		{
			const short xi = rng.Integer<short>();
			const short yi = rng.Integer<short>();
			const double xd = xi;
			const double yd = yi;
			const TBCD x(xi, 10, 10, 0);
			const TBCD y(yi, 10, 10, 0);
			const TBCD m = x + y;
			EXPECT_TRUE(CompareEpsilon(m.ToDouble(), xd + yd, 0.00001));

			if(HasFailure())
			{
				std::cerr<<"operands were: x = "<<xd<<", y = "<<yd<<"\n\n";
				break;
			}
		}
	}

	TEST(io_bcd, TBCD_Sub_loop)
	{
		TXorShift rng;

		for(unsigned i = 0; i < 1000; i++)
		{
			const short xi = rng.Integer<short>();
			const short yi = rng.Integer<short>();
			const double xd = xi;
			const double yd = yi;
			const TBCD x(xi, 10, 10, 0);
			const TBCD y(yi, 10, 10, 0);
			const TBCD d = x - y;
			EXPECT_TRUE(CompareEpsilon(d.ToDouble(), xd - yd, 0.00001));

			if(HasFailure())
			{
				std::cerr<<"operands were: x = "<<xd<<", y = "<<yd<<"\n\n";
				break;
			}
		}
	}

	TEST(io_bcd, TBCD_Multiply_loop)
	{
		TXorShift rng;

		for(unsigned i = 0; i < 1000; i++)
		{
			const short xi = rng.Integer<short>();
			const short yi = rng.Integer<short>();
			const double xd = xi;
			const double yd = yi;
			const TBCD x(xi, 10, 10, 0);
			const TBCD y(yi, 10, 10, 0);
			const TBCD m = x * y;
			EXPECT_TRUE(CompareEpsilon(m.ToDouble(), xd * yd, 0.00001));

			if(HasFailure())
			{
				std::cerr<<"operands were: x = "<<xd<<", y = "<<yd<<"\n\n";
				break;
			}
		}

		for(unsigned i = 0; i < 100; i++)
		{
			const double xd = rng.Float(true) * 100.0;
			const double yd = rng.Float(true) * 100.0;
			const TBCD x(xd, 10, 6, 10);
			const TBCD y(yd, 10, 6, 10);
			const TBCD m = x * y;
			EXPECT_TRUE(CompareEpsilon(m.ToDouble(), xd * yd, 0.00001));

			if(HasFailure())
			{
				std::cerr<<"operands were: x = "<<xd<<", y = "<<yd<<"\n\n";
				break;
			}
		}
	}

	TEST(io_bcd, TBCD_Divide_loop)
	{
		TXorShift rng;

		for(unsigned i = 0; i < 1000; i++)
		{
			const double xd = round(rng.Float(true) * 100.0 * 100.0) / 100.0;
			const double yd = round(rng.Float(true) * 100.0 * 100.0) / 100.0;
			if(yd == 0) continue;
			const TBCD x(xd, 10, 4, 8);
			const TBCD y(yd, 10, 4, 8);
			const TBCD d = x / y;
			EXPECT_TRUE(CompareEpsilon(d.ToDouble(), xd / yd, 0.00001));

			if(HasFailure())
			{
				std::cerr<<"operands were: x = "<<xd<<", y = "<<yd<<"\n\n";
				break;
			}
		}
	}
}
