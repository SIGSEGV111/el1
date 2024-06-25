#include "dev_gpio_dcf77.hpp"
#include "util_bits.hpp"
#include <string.h>

namespace el1::dev::gpio::dcf77
{
	const static system::time::TTime T_BIT_VALUE     = 0.160;
	const static system::time::TTime T_TOO_SHORT_ERR = 0.090;
	const static system::time::TTime T_TOO_LONG_ERR  = 0.300;

	bool DEBUG = false;
	#define IF_DEBUG if(EL_UNLIKELY(DEBUG))

	/*
		Mon 1 => 1
		Di  2 => 2
		Mit 3 => 3
		Don 4 => 4
		Fri 5 => 5
		Sat 6 => 6
		Sun 7 => 0
	*/

	/** @return -3 => gpio error, -2 => signal timing error, -1 => timeout, 0 => "0", 1 => "1" */
	int TDCF77::RecBit(system::time::TTime* const ts_pulse)
	{
		using namespace system::time;

		signal->Trigger(ETrigger::RISING_EDGE);
		signal->State(); // clear IRQ

		IF_DEBUG fprintf(stderr, "TDCF77::RecBit(): waiting ... ");
		if(signal->OnInputTrigger().WaitFor(1) && signal->State() == true)
		{
			const TTime ts_start = TTime::Now(EClock::MONOTONIC);
			signal->Trigger(ETrigger::FALLING_EDGE);
			if(signal->OnInputTrigger().WaitFor(0.5) && signal->State() == false)
			{
				const TTime ts_end = TTime::Now(EClock::MONOTONIC);
				const TTime delta = ts_end - ts_start;
				IF_DEBUG fprintf(stderr, "%lldms ... ", delta.ConvertToI(EUnit::MILLISECONDS));
				if(delta < T_TOO_SHORT_ERR || delta > T_TOO_LONG_ERR)
				{
					IF_DEBUG fprintf(stderr, "error -2\n");
					on_tick.Raise(ts_start, -2);
					return -2;
				}

				const bool v = delta >= T_BIT_VALUE;
				IF_DEBUG fprintf(stderr, "ok, value = %d\n", v ? 1 : 0);
				if(ts_pulse)
					*ts_pulse = ts_start;
				on_tick.Raise(ts_start, v ? 1 : 0);
				return v ? 1 : 0;
			}
			else
			{
				IF_DEBUG fprintf(stderr, "error -3\n");
				on_tick.Raise(ts_start, -3);
				return -3;
			}
		}

		IF_DEBUG fprintf(stderr, "timeout\n");
		on_tick.Raise(0, -1);
		return -1;
	}

	void TDCF77::WaitForSync()
	{
		IF_DEBUG fprintf(stderr, "TDCF77::WaitForSync(): begin wait\n");
		while(RecBit(nullptr) != -1);
		IF_DEBUG fprintf(stderr, "TDCF77::WaitForSync(): synchronized!\n");
	}

	dcf77_msg_t TDCF77::RecMsg(system::time::TTime* const ts_pulse)
	{
		dcf77_msg_t msg;
		msg.bits = 0;

		IF_DEBUG fprintf(stderr, "TDCF77::RecMsg(): begin loop\n");
		for(unsigned i = 0; i < 59; i++)
		{
			const int b = RecBit(ts_pulse);
			if(b < 0)
			{
				msg.bits = 0;
				break;
			}

			msg.bits |= (b ? 0x8000000000000000ULL : 0);
			msg.bits >>= 1;
		}
		msg.bits >>= (64-59);
		IF_DEBUG fprintf(stderr, "TDCF77::RecMsg(): ok, msg = 0x%016llx\n", msg.bits);

		return msg;
	}

	template<typename ... T>
	static bool CheckEvenParity(T ... a)
	{
		return (util::bits::CountOneBits(a...) & 1) == 0;
	}

	bool TDCF77::IsMsgValid(const dcf77_msg_t& msg) const
	{
		if(msg.bits == 0)
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, all bits are zero\n");
			return false;
		}

		if(msg.s == 0)
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, msg.s is zero\n");
			return false;
		}

		if(msg.z1 && msg.z2)
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, both msg.z1 and msg.z2 are set\n");
			return false;
		}

		if(msg.min0 > 9 || msg.min1 > 5)
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, min invalid\n");
			return false;
		}

		if(msg.hour0 > 9 || msg.hour1 > 2 || (msg.hour1 == 2 && msg.hour0 > 4))
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, hour invalid\n");
			return false;
		}

		if(msg.dom0 > 9 || msg.dom1 > 3 || (msg.dom1 == 3 && msg.dom0 > 1))
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, dom invalid\n");
			return false;
		}

		if(msg.dow == 0 || msg.dow > 7)
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, dow invalid\n");
			return false;
		}

		if(msg.month1 == 1 && msg.month0 > 2)
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, month invalid\n");
			return false;
		}

		if(!CheckEvenParity(msg.min0, msg.min1, msg.p1))
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, min parity fail\n");
			return false;
		}

		if(!CheckEvenParity(msg.hour0, msg.hour1, msg.p2))
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, hour parity fail\n");
			return false;
		}

		if(!CheckEvenParity(msg.dom0, msg.dom1, msg.dow, msg.month0, msg.month1, msg.year0, msg.year1, msg.p3))
		{
			IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): error, date parity fail\n");
			return false;
		}

		IF_DEBUG fprintf(stderr, "TDCF77::IsMsgValid(): ok\n");

		return true;
	}

	::tm TDCF77::TranslateMsg(const dcf77_msg_t& msg) const
	{
		::tm t;
		memset(&t, 0, sizeof(t));

		t.tm_sec = 58;
		t.tm_min = msg.min1 * 10 + msg.min0;
		t.tm_min--;
		t.tm_hour = msg.hour1 * 10 + msg.hour0;
		t.tm_mday = msg.dom1 * 10 + msg.dom0;
		t.tm_mon = msg.month1 * 10 + msg.month0;
		t.tm_mon--;
		t.tm_year = (2000 - 1900) + msg.year1 * 10 + msg.year0;	// please update this once we get close to year 3000, thanks.
		t.tm_wday = msg.dow == 7 ? 0 : msg.dow;
		t.tm_yday = -1;
		t.tm_isdst = msg.z1;
		t.tm_gmtoff = msg.z1 ? 7200 : 3600;
		t.tm_zone = msg.z1 ? "CEST" : "CET";

		return t;
	}

	void TDCF77::WorkerMain()
	{
		using namespace system::time;
		TTime ts_pulse;

		for(;;)
		{
			WaitForSync();
			const dcf77_msg_t msg = RecMsg(&ts_pulse);

			if(IsMsgValid(msg))
			{
				const ::tm t = TranslateMsg(msg);
				const TTime ts_now = TTime::Now(EClock::MONOTONIC);
				const u64_t ns = (u64_t)(distance / LIGHTSPEED * 1000000000.0) + (ts_now - ts_pulse).ConvertToI(EUnit::NANOSECONDS);
				on_update.Raise(t, ns, msg.a2);
			}
		}
	}

	TDCF77::TDCF77(std::unique_ptr<IPin> signal, const double distance) :
		signal(std::move(signal)),
		distance(distance),
		on_update(),
		fib_worker(util::function::TFunction<void>(this, &TDCF77::WorkerMain))
	{
		this->signal->Mode(EMode::INPUT);
		this->signal->Trigger(ETrigger::RISING_EDGE);
	}
}
