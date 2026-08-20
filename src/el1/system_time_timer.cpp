#include "system_time_timer.hpp"
#include "io_collection_array.hpp"

namespace el1::system::time::timer
{
	using namespace system::waitable;

	io::collection::array::array_t<const THandleWaitable*> TTimeWaitable::HandleWaitables() const
	{
		if(timer == nullptr)
		{
			timer = New<TTimer>(clock, ts_wait_until, TTime(0));
		}

		return timer->OnTick().HandleWaitables();
	}

	bool TTimeWaitable::IsReady() const
	{
		if(timer == nullptr)
			return TTime::Now(clock) >= ts_wait_until;
		else
			return timer->OnTick().IsReady();
	}
}
