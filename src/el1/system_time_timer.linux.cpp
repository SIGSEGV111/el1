#include "def.hpp"
#ifdef EL_OS_LINUX

#include "system_time_timer.hpp"
#include "error.hpp"
#include <unistd.h>
#include <sys/timerfd.h>
#include <errno.h>

namespace el1::system::time::timer
{
	using namespace system::task;

	usys_t TTimer::ReadMissedTicksCount()
	{
		u64_t count = 0;
		const ssize_t r = read(waitable.Handle(), &count, 8);
		if(r == 8)
		{
			return count;
		}
		else
		{
			if(errno == EAGAIN)
				return 0;
			else
				EL_THROW(TSyscallException, errno);
		}
	}

	void TTimer::Start(const TTime interval, const ETimerMode mode)
	{
		int flags = TFD_TIMER_CANCEL_ON_SET;

		::itimerspec its = {};
		its.it_interval = interval;
		its.it_value = interval;

		switch(mode)
		{
			case ETimerMode::RELATIVE:
				break;

			case ETimerMode::ABSOLUTE:
				flags |= TFD_TIMER_ABSTIME;
				break;
		}


		while(ReadMissedTicksCount() > 0);
		EL_SYSERR(timerfd_settime(waitable.Handle(), flags, &its, nullptr));
	}

	void TTimer::Start(const TTime ts_expire, const TTime interval)
	{
		int flags = TFD_TIMER_CANCEL_ON_SET;

		::itimerspec its = {};
		its.it_interval = interval;
		its.it_value = ts_expire;

		flags |= TFD_TIMER_ABSTIME;

		while(ReadMissedTicksCount() > 0);
		EL_SYSERR(timerfd_settime(waitable.Handle(), flags, &its, nullptr));
	}

	void TTimer::Stop()
	{
		::itimerspec its = {};
		EL_SYSERR(timerfd_settime(waitable.Handle(), 0, &its, nullptr));
	}

	TTimer::~TTimer()
	{
		close(waitable.Handle());
	}

	void TTimer::Init(const EClock clock)
	{
		int clock_id;
		switch(clock)
		{
			case EClock::REALTIME:
				clock_id = CLOCK_REALTIME;
				break;

			case EClock::MONOTONIC:
				clock_id = CLOCK_MONOTONIC;
				break;

			default:
				EL_THROW(TInvalidArgumentException, "clock", "unsupported clock value - only MONOTONIC and REALTIME are supported");
		}

		const fd_t fd = EL_SYSERR(timerfd_create(clock_id, TFD_NONBLOCK | TFD_CLOEXEC));
		waitable.Handle(fd);
	}

	TTimer::TTimer(const EClock clock) : waitable({.read = true, .write = false, .other = false})
	{
		Init(clock);
	}

	TTimer::TTimer(const EClock clock, const TTime ts_expire, const TTime interval) : waitable({.read = true, .write = false, .other = false})
	{
		Init(clock);
		Start(ts_expire, interval);
	}

	TTimer::TTimer(const EClock clock, const TTime interval, const ETimerMode mode) : waitable({.read = true, .write = false, .other = false})
	{
		Init(clock);
		Start(interval, mode);
	}
}

#endif
