#include <el1/el1.cpp>
#include <stdio.h>
#include <math.h>

using namespace el1::io::types;
using namespace el1::io::collection::list;
using namespace el1::dev::spi;
using namespace el1::dev::spi::native;
using namespace el1::dev::spi::hx711;
using namespace el1::dev::gpio;
using namespace el1::dev::gpio::native;
using namespace el1::error;
using namespace el1::system::cmdline;
using namespace el1::system::time;

int main(int argc, char* argv[])
{
	int exit_code = 0;

	try
	{
		s64_t config = 0;
		s64_t irq_pin = -1;
		s64_t ce_pin = -1;
		s64_t hz_clock = HZ_CLOCK_SPI_MAX / 10;
		s64_t n_capture = -1;
		s64_t n_avg = 32;
		double sr = 11.0f;
		el1::dev::spi::hx711::DEBUG = false;
		ParseCmdlineArguments(argc, argv,
			TIntegerArgument(&n_capture, 'n', "count", "", true, false, "number of data samples to capture before quitting (-1 = no limit)"),
			TIntegerArgument(&config, 'c', "config", "", true, false, "0=Channel A with x128 gain, 1=Channel B with x32 gain, 2=Channel A with x64 gain"),
			TIntegerArgument(&ce_pin, 'e', "ce", "", true, false, "The GPIO pin that is used as chip-enable control (-1 = none)"),
			TIntegerArgument(&irq_pin, 'i', "irq", "", true, false, "The GPIO pin that is used as interrupt source (-1 = none)"),
			TIntegerArgument(&hz_clock, 'f', "clock", "", true, false, "The SPI clock frequency in Hz for data transfers"),
			TFloatArgument(&sr, 'r', "rate", "", true, false, "The poll-rate of the test program. The HX711 usually has a sample-rate of ~10Hz or ~80Hz (depending on the speed of the HX711's clock and the state of the RATE-pin). This setting optimizes internal timers and is only used when no IRQ is available. The poll-rate should be a bit higher than the sample-rate."),
			TFlagArgument(&el1::dev::spi::hx711::DEBUG, 'd', "debug", "", "enable debug output"),
			TIntegerArgument(&n_avg, 'a', "avg", "", true, false, "Number of samples for gliding average computation")
		);

		EL_ERROR(config < 0 || config > 2, TInvalidArgumentException, "config", "config must be in the range 0-2");
		EL_ERROR(n_capture < -1, TInvalidArgumentException, "count", "count must >= -1");
		EL_ERROR(ce_pin < -1, TInvalidArgumentException, "ce", "CE pin# must be >= -1");
		EL_ERROR(irq_pin < -1, TInvalidArgumentException, "irq", "IRQ pin# must be >= -1");
		EL_ERROR(hz_clock < HZ_CLOCK_SPI_MIN || hz_clock > HZ_CLOCK_SPI_MAX, TInvalidArgumentException, "clock", "Clock frequency must be in the range 40KHz to 10MHz");
		EL_ERROR(sr <= 0, TInvalidArgumentException, "rate", "The sample-rate must be a positive number");
		EL_ERROR(n_avg < 0, TInvalidArgumentException, "avg", "The number of samples for the gliding average must be >= 0");

		TList<float> arr_avg(n_avg);
		arr_avg.SetCount(n_avg);
		ssys_t idx_avg = 0;

		TNativeGpioController& gpioctrl = *TNativeGpioController::Instance();
		TNativeSpiBus spibus("/dev/spidev0.0");
		THX711 hx711(spibus.ClaimDevice(ce_pin >= 0 ? gpioctrl.ClaimPin(ce_pin) : nullptr), irq_pin >= 0 ? gpioctrl.ClaimPin(irq_pin) : nullptr, hz_clock, sr);

		TTime ts_last;
		float value = 0.0f;
		double f_avg = NAN;
		hx711.Configuration((THX711::EConfig)config);
		hx711.ReadAll(&value, 1);

		for(s64_t i = 0; n_capture == -1 || i < n_capture; i++)
		{
			hx711.ReadAll(&value, 1);
			const TTime ts_now = TTime::Now(EClock::MONOTONIC);
			const TTime t_delta = ts_now - ts_last;
			ts_last = ts_now;

			if(n_avg >= 1)
			{
				arr_avg[idx_avg] = value;
				idx_avg++;
				if(idx_avg >= n_avg)
					idx_avg = 0;

				f_avg = arr_avg[0];
				for(ssys_t j = 1; j < n_avg; j++)
					f_avg += (double)arr_avg[j];
				f_avg /= (double)n_avg;
			}

			fprintf(stderr, "%f (f@% 3.3fHz), avg: %f\n", value, 1.0 / t_delta.ConvertToF(EUnit::SECONDS), f_avg);
		}
	}
	catch(shutdown_t)
	{
		fprintf(stderr, "\nBYE!\n");
	}
	catch(const IException& e)
	{
		e.Print("TOP LEVEL");
		exit_code = 1;
	}

	return exit_code;
}
