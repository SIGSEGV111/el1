#include <gtest/gtest.h>
#include <el1/system_time.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::system::time;

	static const u64_t ATTOS_PS  = 1000000000000000000;
	static const u64_t NANOS_PS  = 1000000000;
	static const u64_t MICROS_PS = 1000000;

	TEST(system_time, TTime_Construct)
	{
		TTime t1;
		TTime t2(0);
		TTime t3(0U);
		TTime t4(0LL,0LL);
		TTime t5(0.2176);
		TTime t6(0.6528);
		TTime t7(0.5000);
		TTime t8(2.048000000000000040);

		EXPECT_EQ(t1, 0);
		EXPECT_EQ(t2, 0);
		EXPECT_EQ(t3, 0);
		EXPECT_EQ(t4, 0);
		EXPECT_EQ(t5, TTime(0,217600000000000000ULL));
		EXPECT_EQ(t6, TTime(0,652800000000000000ULL));
		EXPECT_EQ(t7, TTime(0,500000000000000000ULL));
		EXPECT_EQ(t8, TTime(2, 48000000000000040ULL));
	}

	TEST(system_time, TTime_Compare)
	{
		EXPECT_LE(TTime(10), TTime(20));
		EXPECT_GE(TTime(20), TTime(10));
		EXPECT_LT(TTime(10), TTime(20));
		EXPECT_GT(TTime(20), TTime(10));

		EXPECT_LE(TTime(10,1), TTime(10,2));
		EXPECT_GE(TTime(10,2), TTime(10,1));
		EXPECT_LT(TTime(10,1), TTime(10,2));
		EXPECT_GT(TTime(10,2), TTime(10,1));

		EXPECT_FALSE(TTime(10) >  TTime(20));
		EXPECT_FALSE(TTime(20) <  TTime(10));
		EXPECT_FALSE(TTime(10) >= TTime(20));
		EXPECT_FALSE(TTime(20) <= TTime(10));

		EXPECT_EQ(TTime(1, 10), TTime(1, 10));
		EXPECT_EQ(TTime(1,  0), TTime(1,  0));
		EXPECT_NE(TTime(1, 10), TTime(2, 10));
		EXPECT_NE(TTime(1, 10), TTime(1, 0));

		EXPECT_FALSE(TTime(1, 10) == TTime(2, 10));
		EXPECT_FALSE(TTime(1, 10) == TTime(1,  9));

		EXPECT_FALSE(TTime(2, 10) != TTime(2, 10));
	}

	TEST(system_time, TTime_Math)
	{
		EXPECT_EQ(TTime(10), TTime(5,1) + TTime(4,ATTOS_PS - 1));
		EXPECT_NE(TTime(10), TTime(5,1) + TTime(4,ATTOS_PS - 2));

		EXPECT_EQ(TTime(4,0), TTime(5,1) - TTime(1,1));
		EXPECT_NE(TTime(4,0), TTime(5,1) - TTime(1,2));
		EXPECT_EQ(TTime(4,ATTOS_PS - 1), TTime(5,1) - TTime(0,2));
		EXPECT_EQ(TTime(5,1), TTime(4,ATTOS_PS - 1) + TTime(0,2));

		EXPECT_EQ(TTime(5,0), TTime(7,0) - TTime(0, ATTOS_PS * 2));
		EXPECT_EQ(TTime(5,0), TTime(7,0) + TTime(0, -ATTOS_PS * 2));
		EXPECT_EQ(TTime(7,0), TTime(5,0) + TTime(0, ATTOS_PS * 2));

		EXPECT_EQ(TTime(7,0), TTime(7,ATTOS_PS / 2) + TTime(-1, ATTOS_PS / 2));

		EXPECT_EQ(TTime(1,1) * 5.0, TTime(5,5));
		EXPECT_EQ(TTime(6,6) / 6.0, TTime(1,1));
		EXPECT_EQ(TTime(1,0) / 2.0, TTime(0,ATTOS_PS/2));

		EXPECT_EQ(TTime(1,ATTOS_PS/2) * 2LL, TTime(3,0));
		EXPECT_EQ(TTime(1,ATTOS_PS/2+1) * 2LL, TTime(3,2));
		EXPECT_EQ(TTime(1,ATTOS_PS/2+1) * -2LL, TTime(-3,-2));
		EXPECT_EQ(TTime(-3,-2) / -2LL, TTime(1,ATTOS_PS/2+1));
	}

	TEST(system_time, TTime_Convert)
	{
		EXPECT_EQ(TTime(1,ATTOS_PS/2).ConvertToI(EUnit::MILLISECONDS), 1500LL);
		EXPECT_EQ(TTime(1,ATTOS_PS/2).ConvertToF(EUnit::MILLISECONDS), 1500.0);

		EXPECT_EQ(TTime(60,ATTOS_PS/2).ConvertToI(EUnit::MINUTES), 1LL);
		EXPECT_EQ(TTime(59,ATTOS_PS-1).ConvertToI(EUnit::MINUTES), 0LL);
		EXPECT_EQ(TTime(60,ATTOS_PS/2).ConvertToF(EUnit::MINUTES), 1.0 + 0.5/60.0);

		EXPECT_EQ(TTime(60,ATTOS_PS/2).ConvertToI(3), 181);
		EXPECT_EQ(TTime(60,ATTOS_PS/2).ConvertToF(3), 181.5);

		EXPECT_EQ(TTime::ConvertFrom(EUnit::MINUTES, 1.5), TTime(90,0));
		EXPECT_EQ(TTime::ConvertFrom(EUnit::MINUTES, 2LL), TTime(120,0));

		EXPECT_EQ(TTime::ConvertFrom(EUnit::MILLISECONDS, 1.5), TTime(0,1500000000000000));
		EXPECT_EQ(TTime::ConvertFrom(EUnit::MILLISECONDS, 2LL), TTime(0,2000000000000000));

		EXPECT_EQ(TTime::ConvertFrom(3, 1.5), TTime(0,ATTOS_PS/2));
		EXPECT_EQ(TTime::ConvertFrom(3, 2LL), TTime(0,ATTOS_PS*2/3));
	}

	TEST(system_time, TTime_timespec)
	{
		const timespec ts = { 1, NANOS_PS/2 };
		const timespec c = TTime(1.5);
		EXPECT_TRUE(memcmp(&c, &ts, sizeof(c)) == 0);
		EXPECT_EQ( TTime(1.5), TTime(ts) );
	}

	TEST(system_time, TTime_timeval)
	{
		const timeval ts = { 1, MICROS_PS/2 };
		const timeval c = TTime(1.5);
		EXPECT_TRUE(memcmp(&c, &ts, sizeof(c)) == 0);
		EXPECT_EQ( TTime(1.5), TTime(ts) );
	}

	TEST(system_time, TTime_Now)
	{
		{
			const TTime t1 = TTime::Now(EClock::REALTIME);
			usleep(10);
			const TTime t2 = TTime::Now(EClock::REALTIME);
			EXPECT_GT(t2, t1);
		}

		{
			const TTime t1 = TTime::Now(EClock::MONOTONIC);
			usleep(10);
			const TTime t2 = TTime::Now(EClock::MONOTONIC);
			EXPECT_GT(t2, t1);
		}

		{
			const TTime t1 = TTime::Now(EClock::PROCESS);
			usleep(10);
			const TTime t2 = TTime::Now(EClock::PROCESS);
			EXPECT_GT(t2, t1);
		}

		{
			const TTime t1 = TTime::Now(EClock::THREAD);
			usleep(10);
			const TTime t2 = TTime::Now(EClock::THREAD);
			EXPECT_GT(t2, t1);
		}

		{
			const TTime t1 = TTime::Now(EClock::PROCESS_USER);
			while(TTime::Now(EClock::PROCESS_USER) == t1);
			const TTime t2 = TTime::Now(EClock::PROCESS_USER);
			EXPECT_GT(t2, t1);
		}

		{
			const TTime t1 = TTime::Now(EClock::THREAD_USER);
			while(TTime::Now(EClock::THREAD_USER) == t1);
			const TTime t2 = TTime::Now(EClock::THREAD_USER);
			EXPECT_GT(t2, t1);
		}

		{
			const TTime t1 = TTime::Now(EClock::PROCESS_SYS);
			while(TTime::Now(EClock::PROCESS_SYS) == t1);
			const TTime t2 = TTime::Now(EClock::PROCESS_SYS);
			EXPECT_GT(t2, t1);
		}

		{
			const TTime t1 = TTime::Now(EClock::THREAD_SYS);
			while(TTime::Now(EClock::THREAD_SYS) == t1);
			const TTime t2 = TTime::Now(EClock::THREAD_SYS);
			EXPECT_GT(t2, t1);
		}
	}
}
