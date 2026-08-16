#include <el1/dev_gpio_native.hpp>
#include <el1/dev_spi_hx711.hpp>
#include <el1/dev_spi_native.hpp>
#include <el1/error.hpp>
#include <el1/io_collection_list.hpp>
#include <el1/io_file.hpp>
#include <el1/system_cmdline.hpp>
#include <el1/system_time.hpp>

#include <cmath>
#include <cstdio>
#include <memory>

int main(const int argc, char* argv[])
{
	using namespace el1;
	using namespace el1::dev::gpio;
	using namespace el1::dev::gpio::native;
	using namespace el1::dev::spi::hx711;
	using namespace el1::dev::spi::native;
	using namespace el1::error;
	using namespace el1::io::collection::list;
	using namespace el1::io::file;
	using namespace el1::io::types;
	using namespace el1::system::cmdline;
	using namespace el1::system::time;

	try
	{
		TPath path_spi = "/dev/spidev0.0";
		TPath path_gpio_chip = "/dev/gpiochip0";
		s64_t configuration = 0;
		s64_t irq_line = -1;
		s64_t chip_enable_line = -1;
		s64_t clock_hz = HZ_CLOCK_SPI_MAX / 10;
		s64_t capture_count = -1;
		s64_t average_count = 32;
		f64_t sample_rate = 11.0;

		ParseCmdlineArguments(argc, argv,
			THelpArgument(U"Read samples from an HX711 through an SPI controller."),
			TPathArgument(&path_spi, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'b', U"bus", U"", true, false, U"SPI device"),
			TPathArgument(&path_gpio_chip, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'G', U"gpio-chip", U"", true, false, U"GPIO character device for optional CE/IRQ lines"),
			TIntegerArgument(&capture_count, 'n', U"count", U"", true, false, U"Number of samples; -1 has no limit"),
			TIntegerArgument(&configuration, 'c', U"config", U"", true, false, U"0=channel A x128, 1=channel B x32, 2=channel A x64"),
			TIntegerArgument(&chip_enable_line, 'e', U"ce", U"", true, false, U"GPIO line used as chip enable; -1 disables it"),
			TIntegerArgument(&irq_line, 'i', U"irq", U"", true, false, U"GPIO line used as interrupt source; -1 disables it"),
			TIntegerArgument(&clock_hz, 'f', U"clock", U"", true, false, U"SPI clock frequency in Hz"),
			TFloatArgument(&sample_rate, 'r', U"rate", U"", true, false, U"Expected HX711 sample rate used for polling"),
			TFlagArgument(&DEBUG, 'd', U"debug", U"", U"Enable driver debug output"),
			TIntegerArgument(&average_count, 'a', U"avg", U"", true, false, U"Number of samples in the moving average; 0 disables averaging")
		);

		EL_ERROR(configuration < 0 || configuration > 2, TInvalidArgumentException, "config", "range 0-2");
		EL_ERROR(capture_count < -1, TInvalidArgumentException, "count", "-1 or a non-negative count");
		EL_ERROR(chip_enable_line < -1, TInvalidArgumentException, "ce", "-1 or a non-negative GPIO line");
		EL_ERROR(irq_line < -1, TInvalidArgumentException, "irq", "-1 or a non-negative GPIO line");
		EL_ERROR(clock_hz < HZ_CLOCK_SPI_MIN || clock_hz > HZ_CLOCK_SPI_MAX, TInvalidArgumentException, "clock", "40 kHz to 10 MHz");
		EL_ERROR(sample_rate <= 0, TInvalidArgumentException, "rate", "positive sample rate");
		EL_ERROR(average_count < 0, TInvalidArgumentException, "avg", "non-negative sample count");

		std::unique_ptr<TNativeGpioController> gpio_controller;
		std::unique_ptr<IPin> chip_enable_pin;
		std::unique_ptr<IPin> irq_pin;
		if(chip_enable_line >= 0 || irq_line >= 0)
		{
			gpio_controller = New<TNativeGpioController>(TFile(path_gpio_chip, TAccess::RW));
			if(chip_enable_line >= 0)
			{
				chip_enable_pin = gpio_controller->ClaimPin(static_cast<usys_t>(chip_enable_line));
			}
			if(irq_line >= 0)
			{
				irq_pin = gpio_controller->ClaimPin(static_cast<usys_t>(irq_line));
			}
		}

		TNativeSpiBus spi_bus(path_spi);
		THX711 hx711(
			spi_bus.ClaimDevice(std::move(chip_enable_pin)),
			std::move(irq_pin),
			static_cast<u32_t>(clock_hz),
			static_cast<f32_t>(sample_rate)
		);
		hx711.Configuration(static_cast<THX711::EConfig>(configuration));

		TList<f32_t> average_samples;
		if(average_count > 0)
		{
			average_samples.SetCount(static_cast<usys_t>(average_count));
		}
		usys_t average_index = 0;
		usys_t average_valid = 0;
		TTime timestamp_last = TTime::Now(EClock::MONOTONIC);

		for(s64_t sample_index = 0; capture_count == -1 || sample_index < capture_count; sample_index++)
		{
			f32_t value = 0;
			hx711.ReadAll(&value, 1);
			const TTime timestamp_now = TTime::Now(EClock::MONOTONIC);
			const TTime elapsed = timestamp_now - timestamp_last;
			timestamp_last = timestamp_now;

			f64_t average = NAN;
			if(average_count > 0)
			{
				average_samples[average_index] = value;
				average_index = (average_index + 1U) % average_samples.Count();
				if(average_valid < average_samples.Count())
				{
					average_valid++;
				}

				average = 0;
				for(usys_t i = 0; i < average_valid; i++)
				{
					average += average_samples[i];
				}
				average /= static_cast<f64_t>(average_valid);
			}

			const f64_t elapsed_seconds = elapsed.ConvertToF(EUnit::SECONDS);
			const f64_t frequency = elapsed_seconds > 0 ? 1.0 / elapsed_seconds : INFINITY;
			std::fprintf(stderr, "%f (f@%3.3f Hz), avg: %f\n", static_cast<f64_t>(value), frequency, average);
		}

		return 0;
	}
	catch(const shutdown_t&)
	{
		return 0;
	}
	catch(const IException& exception)
	{
		exception.Print("TOP LEVEL");
		return 1;
	}
}
