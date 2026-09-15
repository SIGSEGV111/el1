#include <gtest/gtest.h>
#include <el1/system_time.hpp>
#include <el1/error.hpp>
#include <limits>
#include <time.h>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::system::time;

	static const u64_t ATTOS_PS  = 1000000000000000000;
	static const u64_t NANOS_PS  = 1000000000;
	static const u64_t MICROS_PS = 1000000;


#ifdef __GLIBC__
	static time_t MakeGlibcUtcTime(const int year, const int month, const int day, const int hour = 0, const int minute = 0, const int second = 0)
	{
		struct tm calendar = {};
		calendar.tm_year = year - 1900;
		calendar.tm_mon = month - 1;
		calendar.tm_mday = day;
		calendar.tm_hour = hour;
		calendar.tm_min = minute;
		calendar.tm_sec = second;
		return timegm(&calendar);
	}

	static void ExpectGregorianCalendarMatchesGlibc(const s64_t timestamp)
	{
		const time_t libc_timestamp = static_cast<time_t>(timestamp);
		ASSERT_EQ(static_cast<s64_t>(libc_timestamp), timestamp) << "time_t cannot represent timestamp " << timestamp;

		struct tm libc_calendar = {};
		ASSERT_NE(gmtime_r(&libc_timestamp, &libc_calendar), nullptr) << "gmtime_r failed for timestamp " << timestamp;

		const TCalendar calendar(TTime(timestamp, 0));
		ASSERT_EQ(calendar.calendar_system, ECalendarSystem::GREGORIAN) << "timestamp=" << timestamp;
		ASSERT_TRUE(
			calendar.year == static_cast<s64_t>(libc_calendar.tm_year) + 1900 &&
			calendar.month == static_cast<unsigned>(libc_calendar.tm_mon + 1) &&
			calendar.day == static_cast<unsigned>(libc_calendar.tm_mday) &&
			calendar.hour == static_cast<unsigned>(libc_calendar.tm_hour) &&
			calendar.minute == static_cast<unsigned>(libc_calendar.tm_min) &&
			calendar.second == static_cast<unsigned>(libc_calendar.tm_sec)
		) << "timestamp=" << timestamp
		  << " el1=" << calendar.year << '-' << static_cast<unsigned>(calendar.month) << '-' << static_cast<unsigned>(calendar.day)
		  << ' ' << static_cast<unsigned>(calendar.hour) << ':' << static_cast<unsigned>(calendar.minute) << ':' << static_cast<unsigned>(calendar.second)
		  << " glibc=" << libc_calendar.tm_year + 1900 << '-' << libc_calendar.tm_mon + 1 << '-' << libc_calendar.tm_mday
		  << ' ' << libc_calendar.tm_hour << ':' << libc_calendar.tm_min << ':' << libc_calendar.tm_sec;

		struct tm reverse = libc_calendar;
		const time_t libc_round_trip = timegm(&reverse);
		ASSERT_EQ(static_cast<s64_t>(libc_round_trip), timestamp) << "glibc round-trip mismatch for timestamp " << timestamp;
		ASSERT_EQ(calendar.ConvertToTime(), TTime(timestamp, 0)) << "el1 round-trip mismatch for timestamp " << timestamp;
	}
#endif

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


	TEST(system_time, TCalendar_UnixEpochAndRoundTrip)
	{
		const TCalendar epoch(TTime(0));
		EXPECT_EQ(epoch.year, 1970);
		EXPECT_EQ(epoch.month, 1);
		EXPECT_EQ(epoch.day, 1);
		EXPECT_EQ(epoch.hour, 0);
		EXPECT_EQ(epoch.minute, 0);
		EXPECT_EQ(epoch.second, 0);
		EXPECT_EQ(epoch.attoseconds, 0U);
		EXPECT_EQ(epoch.calendar_system, ECalendarSystem::GREGORIAN);
		EXPECT_EQ(epoch.ConvertToTime(), TTime(0));
		EXPECT_EQ(TTime(epoch), TTime(0));

		const TCalendar source(2026, 9, 15, 10, 37, 22, 700000000000000000ULL);
		const TTime timestamp = source.ConvertToTime();
		const TCalendar round_trip = timestamp.ConvertToCalendar();
		EXPECT_EQ(round_trip.year, source.year);
		EXPECT_EQ(round_trip.month, source.month);
		EXPECT_EQ(round_trip.day, source.day);
		EXPECT_EQ(round_trip.hour, source.hour);
		EXPECT_EQ(round_trip.minute, source.minute);
		EXPECT_EQ(round_trip.second, source.second);
		EXPECT_EQ(round_trip.attoseconds, source.attoseconds);
		EXPECT_EQ(round_trip.calendar_system, ECalendarSystem::GREGORIAN);
	}

	TEST(system_time, TCalendar_GregorianReform)
	{
		const TCalendar last_julian(1582, 10, 4);
		const TCalendar first_gregorian(1582, 10, 15);
		EXPECT_EQ(last_julian.calendar_system, ECalendarSystem::JULIAN);
		EXPECT_EQ(first_gregorian.calendar_system, ECalendarSystem::GREGORIAN);
		EXPECT_EQ(first_gregorian.ConvertToTime() - last_julian.ConvertToTime(), TTime::ConvertFrom(EUnit::DAYS, 1LL));
		EXPECT_EQ(last_julian.ConvertToTime(), TTime(-12219379200LL, 0));
		EXPECT_EQ(first_gregorian.ConvertToTime(), TTime(-12219292800LL, 0));

		const TCalendar next_day((last_julian.ConvertToTime() + TTime::ConvertFrom(EUnit::DAYS, 1LL)));
		EXPECT_EQ(next_day.year, 1582);
		EXPECT_EQ(next_day.month, 10);
		EXPECT_EQ(next_day.day, 15);
		EXPECT_EQ(next_day.calendar_system, ECalendarSystem::GREGORIAN);

		EXPECT_THROW(TCalendar(1582, 10, 5), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(1582, 10, 10), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(1582, 10, 14), el1::error::TInvalidArgumentException);
	}

	TEST(system_time, TCalendar_LeapYearsAndAncientDates)
	{
		EXPECT_NO_THROW(TCalendar(1500, 2, 29));
		EXPECT_EQ(TCalendar(1500, 2, 29).calendar_system, ECalendarSystem::JULIAN);
		EXPECT_NO_THROW(TCalendar(1600, 2, 29));
		EXPECT_THROW(TCalendar(1700, 2, 29), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(1900, 2, 29), el1::error::TInvalidArgumentException);
		EXPECT_NO_THROW(TCalendar(2000, 2, 29));

		const TCalendar year_zero(0, 2, 29, 12, 34, 56, 123456789012345678ULL);
		EXPECT_EQ(year_zero.calendar_system, ECalendarSystem::JULIAN);
		const TCalendar round_trip(year_zero.ConvertToTime());
		EXPECT_EQ(round_trip.year, 0);
		EXPECT_EQ(round_trip.month, 2);
		EXPECT_EQ(round_trip.day, 29);
		EXPECT_EQ(round_trip.hour, 12);
		EXPECT_EQ(round_trip.minute, 34);
		EXPECT_EQ(round_trip.second, 56);
		EXPECT_EQ(round_trip.attoseconds, 123456789012345678ULL);
	}

	TEST(system_time, TCalendar_NegativeFractionalTimestamp)
	{
		const TTime timestamp(0, -500000000000000000LL);
		const TCalendar calendar(timestamp);
		EXPECT_EQ(calendar.year, 1969);
		EXPECT_EQ(calendar.month, 12);
		EXPECT_EQ(calendar.day, 31);
		EXPECT_EQ(calendar.hour, 23);
		EXPECT_EQ(calendar.minute, 59);
		EXPECT_EQ(calendar.second, 59);
		EXPECT_EQ(calendar.attoseconds, 500000000000000000ULL);
		EXPECT_EQ(calendar.ConvertToTime(), timestamp);
	}

	TEST(system_time, TCalendar_TTimeIntegerRangeRoundTrip)
	{
		const TTime minimum(std::numeric_limits<s64_t>::min(), 0);
		const TTime maximum(std::numeric_limits<s64_t>::max(), 0);
		EXPECT_EQ(minimum.ConvertToCalendar().ConvertToTime(), minimum);
		EXPECT_EQ(maximum.ConvertToCalendar().ConvertToTime(), maximum);

		const TTime below_minimum_fraction(std::numeric_limits<s64_t>::min(), -ATTOS_PS / 2);
		EXPECT_EQ(below_minimum_fraction.ConvertToCalendar().ConvertToTime(), below_minimum_fraction);
	}


	TEST(system_time, TCalendar_FractionalSecondAndDayBoundaries)
	{
		struct TCase
		{
			s64_t seconds;
			s64_t attoseconds;
			s64_t year;
			unsigned month;
			unsigned day;
			unsigned hour;
			unsigned minute;
			unsigned second;
			u64_t calendar_attoseconds;
		};

		static constexpr TCase CASES[] = {
			{ -86400, 0, 1969, 12, 31, 0, 0, 0, 0 },
			{ -86399, 0, 1969, 12, 31, 0, 0, 1, 0 },
			{ -1, 0, 1969, 12, 31, 23, 59, 59, 0 },
			{ 0, -1, 1969, 12, 31, 23, 59, 59, ATTOS_PS - 1 },
			{ 0, 0, 1970, 1, 1, 0, 0, 0, 0 },
			{ 0, 1, 1970, 1, 1, 0, 0, 0, 1 },
			{ 86399, ATTOS_PS - 1, 1970, 1, 1, 23, 59, 59, ATTOS_PS - 1 },
			{ 86400, 0, 1970, 1, 2, 0, 0, 0, 0 },
		};

		for(const TCase& test_case : CASES)
		{
			const TTime timestamp(test_case.seconds, test_case.attoseconds);
			const TCalendar calendar(timestamp);
			EXPECT_EQ(calendar.year, test_case.year);
			EXPECT_EQ(calendar.month, test_case.month);
			EXPECT_EQ(calendar.day, test_case.day);
			EXPECT_EQ(calendar.hour, test_case.hour);
			EXPECT_EQ(calendar.minute, test_case.minute);
			EXPECT_EQ(calendar.second, test_case.second);
			EXPECT_EQ(calendar.attoseconds, test_case.calendar_attoseconds);
			EXPECT_EQ(calendar.ConvertToTime(), timestamp);
		}
	}

	TEST(system_time, TCalendar_DenseRoundTripAcrossGregorianReform)
	{
		const TTime start = TCalendar(1582, 9, 1).ConvertToTime();
		const TTime end = TCalendar(1582, 11, 30, 23, 59, 59).ConvertToTime();
		const TTime step = TTime::ConvertFrom(EUnit::HOURS, 1LL);

		for(TTime timestamp = start; timestamp <= end; timestamp += step)
		{
			const TCalendar calendar(timestamp);
			EXPECT_EQ(calendar.ConvertToTime(), timestamp);
			EXPECT_FALSE(calendar.year == 1582 && calendar.month == 10 && calendar.day >= 5 && calendar.day <= 14);
		}
	}

	TEST(system_time, TCalendar_PosixLeapSecondSemantics)
	{
		const TCalendar before(2016, 12, 31, 23, 59, 59);
		const TCalendar after(before.ConvertToTime() + TTime(1));

		EXPECT_EQ(after.year, 2017);
		EXPECT_EQ(after.month, 1);
		EXPECT_EQ(after.day, 1);
		EXPECT_EQ(after.hour, 0);
		EXPECT_EQ(after.minute, 0);
		EXPECT_EQ(after.second, 0);
		EXPECT_EQ(after.ConvertToTime() - before.ConvertToTime(), TTime(1));
		EXPECT_THROW(TCalendar(2016, 12, 31, 23, 59, 60), el1::error::TInvalidArgumentException);
	}

#ifdef __GLIBC__
	TEST(system_time, TCalendar_GlibcGregorianBoundarySamples)
	{
		static constexpr s64_t TIMESTAMPS[] = {
			-12219292800LL, // 1582-10-15 00:00:00, first Gregorian date used by TCalendar
			-11676096000LL, // 1600-01-01
			-2208988800LL,  // 1900-01-01
			-1LL,
			0LL,
			1LL,
			951782400LL,    // 2000-02-29
			1483228799LL,   // 2016-12-31 23:59:59, immediately before a real UTC leap second
			2147483647LL,
			4107542400LL,   // 2100-03-01
			13574563200LL,  // 2400-02-29
		};

		for(const s64_t timestamp : TIMESTAMPS)
			ExpectGregorianCalendarMatchesGlibc(timestamp);
	}

	TEST(system_time, TCalendar_GlibcGregorianDailyAgreement)
	{
		const s64_t start = static_cast<s64_t>(MakeGlibcUtcTime(1582, 10, 15));
		const s64_t end = static_cast<s64_t>(MakeGlibcUtcTime(2401, 1, 1));
		static constexpr s64_t SECONDS_PER_POSIX_DAY = 86400;

		s64_t day_index = 0;
		for(s64_t day = start; day < end; day += SECONDS_PER_POSIX_DAY, day_index++)
		{
			// Exercise a different time-of-day on each day while still checking every civil date.
			const s64_t second_of_day = (day_index * 7919LL) % SECONDS_PER_POSIX_DAY;
			ExpectGregorianCalendarMatchesGlibc(day + second_of_day);
		}
	}

	TEST(system_time, TCalendar_GlibcIsProlepticGregorianBeforeReform)
	{
		const TCalendar last_julian(1582, 10, 4);
		const s64_t timestamp = last_julian.ConvertToTime().Seconds();
		const time_t libc_timestamp = static_cast<time_t>(timestamp);
		struct tm libc_calendar = {};
		ASSERT_NE(gmtime_r(&libc_timestamp, &libc_calendar), nullptr);

		// glibc gmtime_r uses the proleptic Gregorian calendar here, while TCalendar
		// deliberately follows the historical reform and therefore reports Julian 1582-10-04.
		EXPECT_EQ(libc_calendar.tm_year + 1900, 1582);
		EXPECT_EQ(libc_calendar.tm_mon + 1, 10);
		EXPECT_EQ(libc_calendar.tm_mday, 14);
		EXPECT_EQ(last_julian.calendar_system, ECalendarSystem::JULIAN);

		const time_t glibc_october_4 = MakeGlibcUtcTime(1582, 10, 4);
		EXPECT_EQ(timestamp - static_cast<s64_t>(glibc_october_4), 10LL * 86400LL);

		ExpectGregorianCalendarMatchesGlibc(TCalendar(1582, 10, 15).ConvertToTime().Seconds());
	}
#else
	TEST(system_time, TCalendar_GlibcReferenceTestsUnavailable)
	{
		GTEST_SKIP() << "glibc reference tests require glibc";
	}
#endif

	TEST(system_time, TCalendar_InvalidFields)
	{
		EXPECT_THROW(TCalendar(2026, 0, 1), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(2026, 13, 1), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(2026, 4, 31), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(2026, 1, 1, 24), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(2026, 1, 1, 0, 60), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(2026, 1, 1, 0, 0, 60), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TCalendar(2026, 1, 1, 0, 0, 0, ATTOS_PS), el1::error::TInvalidArgumentException);
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


	TEST(system_time, TTime_timespecNegative)
	{
		const timespec source = { -1, static_cast<long>(NANOS_PS / 2) };
		const TTime timestamp(source);
		const timespec round_trip = timestamp;

		EXPECT_EQ(timestamp, TTime(0, -static_cast<s64_t>(ATTOS_PS / 2)));
		EXPECT_EQ(round_trip.tv_sec, source.tv_sec);
		EXPECT_EQ(round_trip.tv_nsec, source.tv_nsec);

		const timespec sub_nanosecond = TTime(0, -1);
		EXPECT_EQ(sub_nanosecond.tv_sec, 0);
		EXPECT_EQ(sub_nanosecond.tv_nsec, 0);
	}

	TEST(system_time, TTime_timevalNegative)
	{
		const timeval source = { -1, static_cast<suseconds_t>(MICROS_PS / 2) };
		const TTime timestamp(source);
		const timeval round_trip = timestamp;

		EXPECT_EQ(timestamp, TTime(0, -static_cast<s64_t>(ATTOS_PS / 2)));
		EXPECT_EQ(round_trip.tv_sec, source.tv_sec);
		EXPECT_EQ(round_trip.tv_usec, source.tv_usec);

		const timeval sub_microsecond = TTime(0, -1);
		EXPECT_EQ(sub_microsecond.tv_sec, 0);
		EXPECT_EQ(sub_microsecond.tv_usec, 0);
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
