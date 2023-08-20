#include "def.hpp"

#include "system_waitable.hpp"
#include "system_task.hpp"
#include "system_time_timer.hpp"
#include "error.hpp"

namespace el1::system::waitable
{
	using namespace system::task;
	using namespace system::handle;
	using namespace system::time;
	using namespace system::time::timer;

	void IWaitable::WaitFor() const
	{
		EL_ERROR(!WaitFor(-1), error::TLogicException);
	}

	bool IWaitable::WaitFor(const TTime timeout) const
	{
		EL_ERROR(this == nullptr, TLogicException);

		if(timeout >= 0)
		{
			TTimeWaitable time_waitable(EClock::MONOTONIC, TTime::Now(EClock::MONOTONIC) + timeout);

			const IWaitable* array[2] = { this, &time_waitable };
			TFiber::WaitForMany(array_t<const IWaitable*>(array, 2));

			return this->IsReady();
		}
		else
		{
			const IWaitable* array[1] = { this };
			TFiber::WaitForMany(array_t<const IWaitable*>(array, 1));
			return true;
		}
	}
}
