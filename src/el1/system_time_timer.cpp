#include "system_time_timer.hpp"

namespace el1::system::time::timer
{
	using namespace system::waitable;

	const THandleWaitable* TTimeWaitable::HandleWaitable() const
	{
		if(timer == nullptr)
		{
			timer = std::unique_ptr<TTimer>(new TTimer(clock, ts_wait_until, TTime(0)));
		}

		return &timer->OnTick();
	}

	bool TTimeWaitable::IsReady() const
	{
		if(timer == nullptr)
			return TTime::Now(clock) >= ts_wait_until;
		else
			return timer->OnTick().IsReady();
	}
}
