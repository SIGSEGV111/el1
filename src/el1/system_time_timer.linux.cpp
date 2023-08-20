#include "def.hpp"
#ifdef EL_OS_LINUX

#include "system_time_timer.hpp"
#include "error.hpp"
#include <unistd.h>
#include <sys/timerfd.h>
#include <errno.h>

namespace el1::system::time::timer
{
	using namespace system::waitable;
	using namespace system::handle;

	usys_t TTimer::ReadMissedTicksCount()
	{
		waitable.Reset();
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
				EL_THROW(TSyscallException, errno); // This might tigger on a discontinuous negative change to the clock (r will be 0)
		}
	}

	void TTimer::Start(const TTime interval)
	{
		::itimerspec its = {};
		its.it_interval = interval;
		its.it_value = interval;
		EL_SYSERR(timerfd_settime(waitable.Handle(), 0, &its, nullptr));
		waitable.Reset();
	}

	void TTimer::Start(const TTime ts_expire, const TTime interval)
	{
		::itimerspec its = {};
		its.it_interval = interval;
		its.it_value = ts_expire;
		EL_SYSERR(timerfd_settime(waitable.Handle(), TFD_TIMER_ABSTIME, &its, nullptr));
		waitable.Reset();
	}

	void TTimer::Stop()
	{
		::itimerspec its = {};
		EL_SYSERR(timerfd_settime(waitable.Handle(), 0, &its, nullptr));
		waitable.Reset();
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

	TTimer::TTimer(const EClock clock, const TTime interval) : waitable({.read = true, .write = false, .other = false})
	{
		Init(clock);
		Start(interval);
	}
}

#endif
