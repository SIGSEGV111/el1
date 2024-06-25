#include <gtest/gtest.h>
#include <el1/system_random.hpp>
#include <el1/io_collection_map.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::system::random;
	using namespace el1::io::collection::map;

	TEST(system_random, TCMWC)
	{
		TCMWC rng;

		{
			TSortedMap<u32_t, u32_t> histogram;
			rng.Limit(1024).ForEach([&histogram](auto v) { histogram.Get(v, 0)++; });
			for(auto& kv : histogram.Items())
				EXPECT_EQ(kv.value, 1U);
		}
	}

	TEST(system_random, TXorShift)
	{
		{
			TXorShift rng;
			TSortedMap<u64_t, u32_t> histogram;
			rng.Limit(1024).ForEach([&histogram](auto v) { histogram.Get(v, 0)++; });
			for(auto& kv : histogram.Items())
				EXPECT_EQ(kv.value, 1U);
		}
	}

	TEST(system_random, IRNG_IntegerRange)
	{
		TCMWC rng;

		{
			const u8_t RANGE_LOW = 1;
			const u8_t RANGE_HIGH = 6;
			const u8_t RANGE_LEN = RANGE_HIGH - RANGE_LOW + 1;
			const unsigned COUNT = RANGE_LEN * 1000U;

			unsigned histogram[RANGE_LEN] = {};

			for(unsigned i = 0; i < COUNT; i++)
			{
				const u8_t value = rng.IntegerRange<u8_t>(RANGE_LOW, RANGE_HIGH);
				EXPECT_GE(value, RANGE_LOW);
				EXPECT_LE(value, RANGE_HIGH);
				histogram[value - RANGE_LOW]++;
			}

			for(unsigned i = 0; i < RANGE_LEN; i++)
			{
				EXPECT_GE(histogram[i],  900U);
				EXPECT_LE(histogram[i], 1100U);
				// std::cerr<<"histogram["<<i<<"] = "<<histogram[i]<<"\n";
			}
		}

		{
			const u8_t RANGE_LOW = 1;
			const u8_t RANGE_HIGH = 100;
			const u8_t RANGE_LEN = RANGE_HIGH - RANGE_LOW + 1;
			const unsigned COUNT = RANGE_LEN * 1000U;

			unsigned histogram[RANGE_LEN] = {};

			for(unsigned i = 0; i < COUNT; i++)
			{
				const u8_t value = rng.IntegerRange<u8_t>(RANGE_LOW, RANGE_HIGH);
				EXPECT_GE(value, RANGE_LOW);
				EXPECT_LE(value, RANGE_HIGH);
				histogram[value - RANGE_LOW]++;
			}

			for(unsigned i = 0; i < RANGE_LEN; i++)
			{
				EXPECT_GE(histogram[i],  900U);
				EXPECT_LE(histogram[i], 1110U);
				// std::cerr<<"histogram["<<i<<"] = "<<histogram[i]<<"\n";
			}
		}

		/*
		// this demonstrates why you should NOT use modulo to cut ranges from random numbers
		{
			const u64_t RANGE_LOW = 1;
			const u64_t RANGE_HIGH = 100;
			const u64_t RANGE_LEN = RANGE_HIGH - RANGE_LOW + 1;
			const unsigned COUNT = RANGE_LEN * 10000U;

			unsigned histogram[RANGE_LEN] = {};

			for(unsigned i = 0; i < COUNT; i++)
			{
				const u64_t value = rng.Integer<u8_t>() % RANGE_LEN + RANGE_LOW;
				EXPECT_GE(value, RANGE_LOW);
				EXPECT_LE(value, RANGE_HIGH);
				histogram[value - RANGE_LOW]++;
			}

			for(unsigned i = 0; i < RANGE_LEN; i++)
			{
				EXPECT_GE(histogram[i],  9500U);
				EXPECT_LE(histogram[i], 10500U);
				std::cerr<<"histogram["<<i<<"] = "<<histogram[i]<<"\n";
			}
		}
		*/
	}

	TEST(system_random, IRNG_Float)
	{
		TCMWC rng;

		const unsigned n = 100;
		double arr[n];

		{
			for(unsigned i = 0; i < n; i++)
			{
				arr[i] = rng.Float(false);
				EXPECT_GE(arr[i], 0.0);
				EXPECT_LE(arr[i], 1.0);

				for(unsigned j = i; j > 0; j--)
					EXPECT_NE(arr[i], arr[j-1]);
			}
		}

		{
			bool pos = false;
			bool neg = false;
			for(unsigned i = 0; i < n; i++)
			{
				arr[i] = rng.Float(true);
				EXPECT_GE(arr[i], -1.0);
				EXPECT_LE(arr[i],  1.0);

				if(arr[i] > 0)
					pos = true;

				if(arr[i] < 0)
					neg = true;

				for(unsigned j = i; j > 0; j--)
					EXPECT_NE(arr[i], arr[j-1]);
			}
			EXPECT_TRUE(pos);
			EXPECT_TRUE(neg);
		}
	}
}
