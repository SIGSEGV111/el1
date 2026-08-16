#include <el1/dev_i2c_ds2482.hpp>
#include <el1/dev_i2c_native.hpp>
#include <el1/dev_spi_native.hpp>
#include <el1/dev_spi_w1bb.hpp>
#include <el1/dev_w1_ds18x20.hpp>
#include <el1/error.hpp>
#include <el1/io_collection_list.hpp>
#include <el1/io_file.hpp>
#include <el1/io_text_string.hpp>
#include <el1/system_cmdline.hpp>
#include <el1/system_task.hpp>

#include <cstdio>
#include <memory>

static el1::dev::w1::EPowerSource parsePowerSource(const el1::io::text::string::TString& value)
{
	using namespace el1::dev::w1;
	using namespace el1::error;

	if(value == U"auto")
	{
		return EPowerSource::AUTO_DETECT;
	}
	if(value == U"parasitic")
	{
		return EPowerSource::PARASITIC;
	}
	if(value == U"dedicated")
	{
		return EPowerSource::DEDICATED;
	}
	EL_THROW(TInvalidArgumentException, "power", "auto, parasitic or dedicated");
}

int main(const int argc, char* argv[])
{
	using namespace el1;
	using namespace el1::dev::i2c::ds2482;
	using namespace el1::dev::spi::native;
	using namespace el1::dev::spi::w1bb;
	using namespace el1::dev::w1;
	using namespace el1::dev::w1::ds18x20;
	using namespace el1::error;
	using namespace el1::io::collection::list;
	using namespace el1::io::file;
	using namespace el1::io::text::string;
	using namespace el1::system::cmdline;
	using namespace el1::system::task;

	try
	{
		TString backend = U"spi";
		TString power_source_name = U"auto";
		TString raspberry_pi_polling_limit_file;
		TPath spi_device = "/dev/spidev0.0";
		TPath i2c_device = "/dev/i2c-1";
		s64_t i2c_address = 0x18;
		s64_t min_transfer_bytes = 96;
		bool invert_tx = true;
		bool active_pullup = true;

		ParseCmdlineArguments(argc, argv,
			THelpArgument(U"Scan a 1-Wire bus and read DS18B20/DS18S20 temperature sensors. Supports SPI bit-banging and DS2482S-100."),
			TStringArgument(&backend, 'B', U"backend", U"", true, false, U"Bus backend: spi or ds2482"),
			TStringArgument(&power_source_name, 'P', U"power", U"", true, false, U"Sensor power source: auto, parasitic or dedicated"),
			TPathArgument(&spi_device, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 's', U"spi-device", U"", true, false, U"SPI device used by the bit-bang backend"),
			TBooleanArgument(&invert_tx, 'I', U"invert-tx", U"", true, false, U"Invert SPI transmit polarity"),
			TIntegerArgument(&min_transfer_bytes, 'M', U"min-transfer-bytes", U"", true, false, U"Minimum SPI transfer size used to enforce DMA"),
			TStringArgument(&raspberry_pi_polling_limit_file, 'R', U"rpi-polling-limit-file", U"", true, false, U"Optional spi_bcm2835 polling_limit_us file to set to zero"),
			TPathArgument(&i2c_device, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'i', U"i2c-device", U"", true, false, U"I2C device used by the DS2482 backend"),
			TIntegerArgument(&i2c_address, 'a', U"address", U"", true, false, U"DS2482 7-bit I2C address"),
			TBooleanArgument(&active_pullup, 'A', U"active-pullup", U"", true, false, U"Enable the DS2482 active pull-up feature")
		);

		EL_ERROR(backend != U"spi" && backend != U"ds2482", TInvalidArgumentException, "backend", "spi or ds2482");
		EL_ERROR(i2c_address < 0x03 || i2c_address > 0x77, TInvalidArgumentException, "address", "valid 7-bit I2C address");
		EL_ERROR(min_transfer_bytes < 0 || min_transfer_bytes > 255, TInvalidArgumentException, "min-transfer-bytes", "range 0-255");
		const EPowerSource power_source = parsePowerSource(power_source_name);

		std::unique_ptr<TNativeSpiBus> spi_bus;
		std::unique_ptr<el1::dev::i2c::native::TBus> i2c_bus;
		std::unique_ptr<IW1Bus> one_wire_bus;

		if(backend == U"spi")
		{
			if(raspberry_pi_polling_limit_file.Length() > 0)
			{
				TFile(TPath(raspberry_pi_polling_limit_file), TAccess::RW).WriteAll(reinterpret_cast<const byte_t*>("0"), 1);
			}

			spi_bus = New<TNativeSpiBus>(spi_device);
			one_wire_bus = New<TW1BbBus, IW1Bus>(
				spi_bus->ClaimDevice(nullptr),
				invert_tx,
				ESpeed::REGULAR,
				static_cast<u8_t>(min_transfer_bytes)
			);
		}
		else
		{
			i2c_bus = New<el1::dev::i2c::native::TBus>(i2c_device);
			one_wire_bus = New<TDS2482Bus, IW1Bus>(
				i2c_bus->ClaimDevice(static_cast<u8_t>(i2c_address), el1::dev::i2c::ESpeedClass::FAST),
				active_pullup
			);
		}

		std::fprintf(stderr, "scanning bus ... ");
		const TList<uuid_t> found_uuids = one_wire_bus->Scan();
		std::fprintf(stderr, "done\n\nfound devices:\n");

		TList<std::unique_ptr<TDS18X20>> sensors;
		for(usys_t i = 0; i < found_uuids.Count(); i++)
		{
			const TString uuid_string = found_uuids[i].ToString();
			std::fprintf(stderr, "[%2llu] %s\n", static_cast<unsigned long long>(i), uuid_string.MakeCStr().get());
			if(found_uuids[i].type == static_cast<u8_t>(EModel::DS18S20) || found_uuids[i].type == static_cast<u8_t>(EModel::DS18B20))
			{
				sensors.MoveAppend(New<TDS18X20>(one_wire_bus->ClaimDevice(found_uuids[i]), power_source));
			}
		}

		if(sensors.Count() == 0)
		{
			std::fprintf(stderr, "no supported temperature sensors found\n");
			return 0;
		}

		for(auto& sensor : sensors)
		{
			sensor->TriggerConversion();
		}
		TFiber::Sleep(0.75);

		for(auto& sensor : sensors)
		{
			sensor->Fetch();
			const TString model = sensor->ModelName();
			const TString uuid = sensor->Device().UUID().ToString();
			std::fprintf(
				stdout,
				"%s @ %s => %.4f °C\n",
				model.MakeCStr().get(),
				uuid.MakeCStr().get(),
				static_cast<f64_t>(sensor->Temperature())
			);
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
