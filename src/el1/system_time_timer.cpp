#include "system_time_timer.hpp"

namespace el1::system::time::timer
{
	using namespace system::task;

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
		if(TTime::Now(clock) > ts_wait_until)
			return true;

		if(timer != nullptr)
			return timer->OnTick().IsReady();

		return false;
	}
}
