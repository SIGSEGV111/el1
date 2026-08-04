#include <el1/dev_gpio_native.hpp>
#include <el1/dev_i2c_ads111x.hpp>
#include <el1/dev_i2c_native.hpp>
#include <el1/error.hpp>
#include <el1/io_file.hpp>
#include <el1/io_path.hpp>
#include <el1/system_cmdline.hpp>

#include <cstdio>
#include <memory>

int main(const int argc, char* argv[])
{
	using namespace el1;
	using namespace el1::dev::gpio;
	using namespace el1::dev::gpio::native;
	using namespace el1::dev::i2c::ads111x;
	using namespace el1::dev::i2c::native;
	using namespace el1::error;
	using namespace el1::io::file;
	using namespace el1::system::cmdline;

	try
	{
		TPath path_i2c = "/dev/i2c-1";
		TPath path_gpio_chip = "/dev/gpiochip0";
		s64_t i2c_address = 0x48;
		s64_t idx_datarate = 7;
		s64_t idx_pga = 0;
		s64_t idx_channel = 4;
		s64_t idx_opmode = 1;
		s64_t idx_irq = -1;

		ParseCmdlineArguments(argc, argv,
			THelpArgument("Read samples from an ADS111x ADC."),
			TFlagArgument(&DEBUG, 'd', "debug", "", "Enable debug output"),
			TPathArgument(&path_i2c, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'b', "bus", "", true, false, "I2C bus device"),
			TIntegerArgument(&i2c_address, 'a', "address", "", true, false, "7-bit I2C address"),
			TPathArgument(&path_gpio_chip, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'G', "gpio-chip", "", true, false, "GPIO character device used for the optional IRQ"),
			TIntegerArgument(&idx_datarate, 'r', "datarate", "", true, false, "Datarate index 0-7: 8, 16, 32, 64, 128, 250, 475 or 860 samples/s"),
			TIntegerArgument(&idx_pga, 'g', "pga", "", true, false, "PGA index 0-5: 6144, 4096, 2048, 1024, 512 or 256 mV full scale"),
			TIntegerArgument(&idx_channel, 'c', "channel", "", true, false, "Input channel index 0-7"),
			TIntegerArgument(&idx_opmode, 'm', "mode", "", true, false, "Operation mode: 0=continuous, 1=single-shot"),
			TIntegerArgument(&idx_irq, 'i', "irq", "", true, false, "GPIO line index for data-ready IRQ; -1 disables IRQ")
		);

		EL_ERROR(i2c_address < 0x03 || i2c_address > 0x77, TInvalidArgumentException, "address", "valid 7-bit I2C address");
		EL_ERROR(idx_datarate < 0 || idx_datarate > 7, TInvalidArgumentException, "datarate", "range 0-7");
		EL_ERROR(idx_pga < 0 || idx_pga > 5, TInvalidArgumentException, "pga", "range 0-5");
		EL_ERROR(idx_channel < 0 || idx_channel > 7, TInvalidArgumentException, "channel", "range 0-7");
		EL_ERROR(idx_opmode < 0 || idx_opmode > 1, TInvalidArgumentException, "mode", "range 0-1");
		EL_ERROR(idx_irq < -1, TInvalidArgumentException, "irq", "-1 or a non-negative GPIO line index");

		config_t config;
		config.OpMode(static_cast<EOpMode>(idx_opmode));
		config.DataRate(static_cast<EDataRate>(idx_datarate));
		config.PGA(static_cast<EPGA>(idx_pga));
		config.Channel(static_cast<EChannel>(idx_channel));

		if(idx_irq >= 0)
		{
			config.ConfigureDataReadyIrq();
		}
		else
		{
			config.DisableIrq();
		}

		TBus i2c(path_i2c);
		std::unique_ptr<TNativeGpioController> gpio_controller;
		std::unique_ptr<IPin> irq_pin;

		if(idx_irq >= 0)
		{
			gpio_controller = New<TNativeGpioController>(TFile(path_gpio_chip, TAccess::RW));
			irq_pin = gpio_controller->ClaimPin(static_cast<usys_t>(idx_irq));
		}

		TADS111X adc(i2c.ClaimDevice(static_cast<u8_t>(i2c_address)), std::move(irq_pin));
		adc.Config(config);

		if(config.OpMode() == EOpMode::SINGLE_SHOT)
		{
			adc.TriggerConversion();
			s16_t result = 0;
			adc.ReadAll(&result, 1);
			std::printf("%hd\n", result);
		}
		else
		{
			for(;;)
			{
				s16_t result = 0;
				adc.ReadAll(&result, 1);
				std::printf("%hd\n", result);
				std::fflush(stdout);
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
