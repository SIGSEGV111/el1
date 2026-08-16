#include <el1/dev_gpio_native.hpp>
#include <el1/error.hpp>
#include <el1/io_file.hpp>
#include <el1/system_cmdline.hpp>
#include <el1/system_task.hpp>

#include <cstdio>

int main(const int argc, char* argv[])
{
	using namespace el1::dev::gpio;
	using namespace el1::dev::gpio::native;
	using namespace el1::error;
	using namespace el1::io::file;
	using namespace el1::system::cmdline;
	using namespace el1::system::task;

	try
	{
		TPath gpio_chip = "/dev/gpiochip0";
		s64_t gpio_line = -1;
		s64_t delay_ms = 500;
		s64_t repeat_count = 10;
		s64_t mode = 1;

		ParseCmdlineArguments(argc, argv,
			THelpArgument(U"Toggle a GPIO output or alternate its input pull resistor."),
			TPathArgument(&gpio_chip, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'G', U"gpio-chip", U"", true, false, U"GPIO character device"),
			TIntegerArgument(&gpio_line, 'g', U"gpio", U"", false, false, U"GPIO line index"),
			TIntegerArgument(&delay_ms, 'd', U"delay", U"", true, false, U"Delay between state changes in milliseconds"),
			TIntegerArgument(&repeat_count, 'n', U"count", U"", true, false, U"Number of cycles"),
			TIntegerArgument(&mode, 'm', U"mode", U"", true, false, U"0=alternate input pull-down/up, 1=toggle output low/high")
		);

		EL_ERROR(gpio_line < 0, TInvalidArgumentException, "gpio", "non-negative GPIO line index");
		EL_ERROR(delay_ms < 0, TInvalidArgumentException, "delay", "non-negative delay");
		EL_ERROR(repeat_count < 0, TInvalidArgumentException, "count", "non-negative cycle count");
		EL_ERROR(mode < 0 || mode > 1, TInvalidArgumentException, "mode", "0 or 1");

		TNativeGpioController gpio_controller(TFile(gpio_chip, TAccess::RW));
		auto gpio_pin = gpio_controller.ClaimPin(static_cast<usys_t>(gpio_line));
		const f64_t delay_seconds = static_cast<f64_t>(delay_ms) / 1000.0;

		if(mode == 0)
		{
			gpio_pin->Mode(EMode::INPUT);
			for(s64_t i = 0; i < repeat_count; i++)
			{
				gpio_pin->Pull(EPull::DOWN);
				TFiber::Sleep(delay_seconds);
				gpio_pin->Pull(EPull::UP);
				TFiber::Sleep(delay_seconds);
			}
		}
		else
		{
			gpio_pin->Mode(EMode::OUTPUT);
			for(s64_t i = 0; i < repeat_count; i++)
			{
				gpio_pin->State(false);
				TFiber::Sleep(delay_seconds);
				gpio_pin->State(true);
				TFiber::Sleep(delay_seconds);
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
