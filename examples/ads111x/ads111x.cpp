#include <el1/el1.cpp>

int main(int argc, char* argv[])
{
	using namespace el1::error;
	using namespace el1::io::types;
	using namespace el1::io::file;
	using namespace el1::dev::gpio::native;
	using namespace el1::dev::i2c::native;
	using namespace el1::dev::i2c::ads111x;
	using namespace el1::system::cmdline;
	using namespace el1::io::text;
	using namespace el1::io::text::string;
	using namespace el1::io::text::encoding::utf8;

	try
	{
		TPath p_i2c = "/dev/i2c-1";
		s64_t idx_datarate = 7;
		s64_t idx_pga = 0;
		s64_t idx_channel = 4;
		s64_t idx_opmode = 1;
		s64_t idx_irq = -1;

		ParseCmdlineArguments(argc, argv,
			TFlagArgument(&DEBUG, 'd', "debug", "", "Enable debug output"),
			TPathArgument(&p_i2c, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'b', "bus", "", true, false, "The I2C bus device to use"),
			TIntegerArgument(&idx_datarate, 'r', "datarate", "", true, false, "Datarate of the chip. Range 0-7: [0=>8, 1=>16, 2=>32, 3=>64, 4=>128, 5=>250, 6=>475, 7=>860 samples per second]"),
			TIntegerArgument(&idx_pga, 'g', "pga", "", true, false, "PGA (amplifier) setting. Configures the full-scale range of the chip. Range 0-5: [0=>6144mV, 1=>4096mV, 2=>2048mV, 3=>1024mV, 4=>512mV, 5=>256mV]"),
			TIntegerArgument(&idx_channel, 'c', "channel", "", true, false, "Input channel. Range 0-7 [0=>A0-A1, 1=>A0-A3, 2=>A1-A3, 3=>A2-A3, 4=>A0, 5=>A1, 6=>A2, 7=>A3]. 0-3 are two-channel differential modes while 4-7 are single channel absolute modes."),
			TIntegerArgument(&idx_opmode, 'm', "mode", "", true, false, "Operation mode. Range: 0-1 [0=>continuous, 1=>one shot"),
			TIntegerArgument(&idx_irq, 'i', "irq", "", true, false, "Configures the GPIO pin# used for IRQ signaling (-1=>disabled). Allows for faster and more reliable data readout from the chip. The IRQ line must have a pull-up resistor (e.g. 10kOhm to Vcc). The IRQ line can NOT be shared with other devices, since the ADS111X has no data ready flag in its registers.")
		);

		EL_ERROR(idx_datarate < 0 || idx_datarate > 7, TInvalidArgumentException, "datarate", "range 0-7");
		EL_ERROR(idx_pga < 0 || idx_pga > 5, TInvalidArgumentException, "pga", "range 0-5");
		EL_ERROR(idx_channel < 0 || idx_channel > 7, TInvalidArgumentException, "channel", "range 0-7");
		EL_ERROR(idx_opmode < 0 || idx_opmode > 1, TInvalidArgumentException, "opmode", "range 0-1");

		config_t config;
		config.OpMode((EOpMode)idx_opmode);
		config.DataRate((EDataRate)idx_datarate);
		config.PGA((EPGA)idx_pga);
		config.Channel((EChannel)idx_channel);

		if(idx_irq >= 0)
			config.ConfigureDataReadyIrq();
		else
			config.DisableIrq();

		TBus i2c(p_i2c);
		TNativeGpioController& gpio_ctrl = *TNativeGpioController::Instance();
		TADS111X adc(i2c.ClaimDevice(0x48), idx_irq == -1 ? nullptr : gpio_ctrl.ClaimPin(idx_irq));
		adc.Config(config);

		if(config.OpMode() == EOpMode::SINGLE_SHOT)
		{
			adc.TriggerConversion();

			s16_t result;
			adc.ReadAll(&result, 1);
			printf("%hd\n", result);
		}
		else
		{
			s16_t result;
			for(;;)
			{
				adc.ReadAll(&result, 1);
				printf("%hd\n", result);
			}
		}

		fflush(stdout);
		return 0;
	}
	catch(shutdown_t)
	{
		fprintf(stderr, "\nBYE!\n");
		return 0;
	}
	catch(const IException& e)
	{
		e.Print("TOP LEVEL");
		return 1;
	}

	return 2;
}
