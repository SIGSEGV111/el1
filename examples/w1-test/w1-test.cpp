#include <el1/el1.cpp>

using namespace el1::debug;
using namespace el1::error;
using namespace el1::dev::w1;
using namespace el1::dev::w1::ds18x20;
using namespace el1::dev::spi;
using namespace el1::dev::spi::native;
using namespace el1::dev::spi::w1bb;
using namespace el1::system::task;

int main()
{
	try
	{
		TFile("/sys/module/spi_bcm2835/parameters/polling_limit_us", TAccess::RW).Write((const byte_t*)"0", 1);

		TNativeSpiBus spibus("/dev/spidev0.0");
		TW1BbBus w1bus(spibus.ClaimDevice(nullptr), true, ESpeed::REGULAR, 96U);

		std::cerr<<"scanning bus ... ";
		TList<uuid_t> found_uuids = w1bus.Scan();
		std::cerr<<"done\n\n";

		std::cerr<<"found devices:\n";
		TList<std::unique_ptr<TDS18X20>> sensors;
		for(usys_t i = 0; i < found_uuids.Count(); i++)
		{
			std::cerr<<TString::Format("[% 2d] %s\n", i, found_uuids[i].ToString()).MakeCStr().get();
			if(found_uuids[i].type == 0x10 || found_uuids[i].type == 0x28)
				sensors.MoveAppend(std::unique_ptr<TDS18X20>(new TDS18X20(w1bus.ClaimDevice(found_uuids[i]))));
		}
		std::cerr<<std::endl;

		for(auto& sensor : sensors)
			sensor->TriggerConversion();

		TFiber::Sleep(0.75);

		for(auto& sensor : sensors)
		{
			sensor->Fetch();
			std::cerr<<sensor->ModelName().MakeCStr().get()<<" @ "<<sensor->Device().UUID().ToString().MakeCStr().get()<<" => "<<sensor->Temperature()<<" °C\n";
		}

		return 0;
	}
	catch(const IException& e)
	{
		e.Print("terminating due to exception");
		return 1;
	}
	catch(...)
	{
		std::cerr<<"ERROR: unknown exception thrown\n";
		return 2;
	}
}
