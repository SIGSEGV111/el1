#include "def.hpp"
#ifdef EL_OS_CLASS_POSIX

#include "system_time.hpp"
#include "error.hpp"

#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

namespace el1::system::time
{
	TTime TTime::Now(EClock clock)
	{
		timespec ts;
		clockid_t id = -1;
		rusage ru;

		switch(clock)
		{
			case EClock::REALTIME:
				id = CLOCK_REALTIME;
				break;

			case EClock::MONOTONIC:
				id = CLOCK_MONOTONIC;
				break;

			case EClock::PROCESS:
				id = CLOCK_PROCESS_CPUTIME_ID;
				break;

			case EClock::THREAD:
				id = CLOCK_THREAD_CPUTIME_ID;
				break;

			case EClock::PROCESS_USER:
				EL_SYSERR(getrusage(RUSAGE_SELF, &ru));
				return TTime(ru.ru_utime.tv_sec, (s64_t)ru.ru_utime.tv_usec * (s64_t)1000000000000);

			case EClock::PROCESS_SYS:
				EL_SYSERR(getrusage(RUSAGE_SELF, &ru));
				return TTime(ru.ru_stime.tv_sec, (s64_t)ru.ru_stime.tv_usec * (s64_t)1000000000000);

			case EClock::THREAD_USER:
				EL_SYSERR(getrusage(RUSAGE_THREAD, &ru));
				return TTime(ru.ru_utime.tv_sec, (s64_t)ru.ru_utime.tv_usec * (s64_t)1000000000000);

			case EClock::THREAD_SYS:
				EL_SYSERR(getrusage(RUSAGE_THREAD, &ru));
				return TTime(ru.ru_stime.tv_sec, (s64_t)ru.ru_stime.tv_usec * (s64_t)1000000000000);
		}

		EL_SYSERR(clock_gettime(id, &ts));

		return TTime((s64_t)ts.tv_sec, (s64_t)ts.tv_nsec * (s64_t)1000000000);
	}

	TTime::operator timespec() const noexcept
	{
		time_t out_sec = static_cast<time_t>(sec);
		long out_nsec = static_cast<long>(asec / 1000000000LL);
		if(out_nsec < 0)
		{
			out_sec--;
			out_nsec += 1000000000L;
		}

		return timespec { out_sec, out_nsec };
	}

	TTime::operator timeval() const noexcept
	{
		time_t out_sec = static_cast<time_t>(sec);
		long out_usec = static_cast<long>(asec / 1000000000000LL);
		if(out_usec < 0)
		{
			out_sec--;
			out_usec += 1000000L;
		}

		return timeval { out_sec, out_usec };
	}
}

#endif
