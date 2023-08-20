#pragma once

#include "system_time.hpp"
#include "system_waitable.hpp"
#include <memory>

namespace el1::system::time::timer
{
	using namespace time;

	class TTimer
	{
		protected:
			system::waitable::THandleWaitable waitable;
			void Init(const EClock clock);

		public:
			const system::waitable::THandleWaitable& OnTick() const { return waitable; }

			// returns the number of ticks that happend since the last call to ReadMissedTicksCount()/Start()
			// call this function to reset the waitable
			usys_t ReadMissedTicksCount();

			void Start(const TTime interval);
			void Start(const TTime ts_expire, const TTime interval);
			void Stop();

			~TTimer();
			TTimer(const EClock clock);

			// also immediatelly starts the timer
			TTimer(const EClock clock, const TTime ts_expire, const TTime interval);
			TTimer(const EClock clock, const TTime interval);
	};

	struct TTimeWaitable : public waitable::IWaitable
	{
		const EClock clock;
		const TTime ts_wait_until;
		mutable std::unique_ptr<TTimer> timer;

		bool IsReady() const final override;
		const system::waitable::THandleWaitable* HandleWaitable() const final override;

		TTimeWaitable(const EClock clock, const TTime ts_wait_until) : clock(clock), ts_wait_until(ts_wait_until) {}
	};
}
