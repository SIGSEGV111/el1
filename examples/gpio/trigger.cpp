#include <el1/dev_gpio_native.hpp>
#include <el1/error.hpp>
#include <el1/io_file.hpp>
#include <el1/system_cmdline.hpp>

#include <cstdio>

int main(const int argc, char* argv[])
{
	using namespace el1::dev::gpio;
	using namespace el1::dev::gpio::native;
	using namespace el1::error;
	using namespace el1::io::file;
	using namespace el1::system::cmdline;

	try
	{
		TPath gpio_chip = "/dev/gpiochip0";
		s64_t gpio_line = -1;
		s64_t timeout_ms = 1000;
		s64_t repeat_count = 10;
		s64_t debounce_us = 0;

		ParseCmdlineArguments(argc, argv,
			THelpArgument(U"Wait for GPIO edge events through the Linux GPIO character-device API."),
			TPathArgument(&gpio_chip, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'G', U"gpio-chip", U"", true, false, U"GPIO character device"),
			TIntegerArgument(&gpio_line, 'g', U"gpio", U"", false, false, U"GPIO line index"),
			TIntegerArgument(&timeout_ms, 't', U"timeout", U"", true, false, U"Per-event timeout in milliseconds; -1 waits indefinitely"),
			TIntegerArgument(&repeat_count, 'n', U"count", U"", true, false, U"Number of events or timeouts to process"),
			TIntegerArgument(&debounce_us, 'd', U"debounce", U"", true, false, U"Hardware debounce interval in microseconds")
		);

		EL_ERROR(gpio_line < 0, TInvalidArgumentException, "gpio", "non-negative GPIO line index");
		EL_ERROR(timeout_ms < -1, TInvalidArgumentException, "timeout", "-1 or a non-negative timeout");
		EL_ERROR(repeat_count < 0, TInvalidArgumentException, "count", "non-negative iteration count");
		EL_ERROR(debounce_us < 0, TInvalidArgumentException, "debounce", "non-negative debounce interval");

		TNativeGpioController gpio_controller(TFile(gpio_chip, TAccess::RW));
		auto gpio_pin = gpio_controller.ClaimPin(static_cast<usys_t>(gpio_line));
		static_cast<TNativeGpioPin&>(*gpio_pin).Configure(EMode::INPUT, ETrigger::BOTH_EDGES, EPull::DISABLED, static_cast<u32_t>(debounce_us));
		std::printf("state = %d\n", gpio_pin->State() ? 1 : 0);

		for(s64_t i = 0; i < repeat_count; i++)
		{
			std::printf("waiting... ");
			std::fflush(stdout);
			const bool triggered = timeout_ms < 0
				? gpio_pin->OnInputTrigger().WaitFor(-1)
				: gpio_pin->OnInputTrigger().WaitFor(static_cast<f64_t>(timeout_ms) / 1000.0);

			if(triggered)
			{
				gpio_pin->AcknowledgeInputTrigger();
				std::printf("triggered, state = %d\n", gpio_pin->State() ? 1 : 0);
			}
			else
			{
				std::printf("timeout\n");
			}
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
