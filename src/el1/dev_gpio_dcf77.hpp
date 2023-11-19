#pragma once

#include "dev_gpio.hpp"
#include "system_task.hpp"
#include "util_event.hpp"
#include <time.h>

namespace el1::dev::gpio::dcf77
{
	// https://en.wikipedia.org/wiki/DCF77

	static const double LIGHTSPEED = 299710000;	// m/s
	extern bool DEBUG;

	union dcf77_msg_t
	{
		u64_t bits;
		struct
		{
			u64_t
				__nonsense : 14,
				r  : 1,		// abnormal status
				a1 : 1,		// summer time announcement
				z1 : 1,		// timezone: CEST
				z2 : 1,		// timezone: CET
				a2 : 1,		// leap second announcement
				s  : 1,		// start of encoded time (must be 1)
				min0 : 4,	// minutes first digit
				min1 : 3,	// minutes second digit
				p1 : 1,		// even parity over min0 and min1
				hour0 : 4,	// hour first digit
				hour1 : 2,	// hour second digit
				p2 : 1,		// even parity over hour0 and hour1
				dom0 : 4,	// day of month first digit
				dom1 : 2,	// day of month second digit
				dow : 3,	// day of week (1=Monday, 7=Sunday)
				month0 : 4,	// month first digit
				month1 : 1,	// month second digit
				year0 : 4,	// year first digit
				year1 : 4,	// year second digit
				p3 : 1,		// even parity over dom, dow, month and year
				__unused : 4;
		};
	};

	class TDCF77
	{
		protected:
			std::unique_ptr<IPin> signal;
			double distance;
			util::event::TEvent<const ::tm&, u64_t, bool> on_update;
			util::event::TEvent<const system::time::TTime&, int> on_tick;
			system::task::TFiber fib_worker;

			void WorkerMain();
			void WaitForSync();
			dcf77_msg_t RecMsg(system::time::TTime* const ts_pulse);
			bool IsMsgValid(const dcf77_msg_t&) const;
			::tm TranslateMsg(const dcf77_msg_t&) const;
			int RecBit(system::time::TTime* const ts_pulse);

		public:
			/**
			 * Raised after a complete and valid DCF77 message has been received.
			 * The event is *NOT* fired at the full minute/second pulse, but roughly
			 * 100-200ms after the 58th second of the minute. The timestamp is already
			 * compensated for this delay and will be accurate at the time of the event.
			 * @param time Calendar date and time with timezone from the DCF77 message.
			 * @param nanoseconds Nanoseconds to be added to the time.
			 * @param leap_announce Announces the occurance of a leap second ahead of time.
			 */
			const util::event::TEvent<const ::tm&, u64_t, bool>& OnUpdate() const { return on_update; }

			/**
			 * Raised 100-200ms after receiving the per-second-pulse of the DCF77 signal or roughly
			 * 1000ms after the lack of thereof. Check the status code to find out what happend.
			 * @param ts_tick Timestamp of the tick on the monotonic clock. Use to compensate event delay.
			 * This param is only valid for status codes 0 and 1.
			 * @param status Status code of the tick. -3 => gpio error, -2 => signal timing error,
			 * -1 => timeout/sync/aprox. start of new minute, 0 => zero-bit received, 1 => one-bit received
			 */
			const util::event::TEvent<const system::time::TTime&, int>& OnTick() const { return on_tick; }

			/**
			 * Monitors a DCF77 signal in the background using a fiber.
			 * Once a signal has been received and decoded the event OnUpdate() is raised.
			 * Roughly every second OnTick() is raised.
			 *
			 * @param signal The GPIO PIN to monitor for the DCF77 signal.
			 * @param distance The distance in meters from the DCF77 sending station in Mainhausen Germany.
			 * Used to compensate for signal runtime. Set to zero to not compensate.
			 */
			TDCF77(std::unique_ptr<IPin> signal, const double distance = 0);
	};
}
