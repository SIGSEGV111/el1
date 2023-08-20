#include <iostream>
#include <unistd.h>
#include "../../gen/dbg/amalgam/el1.cpp"

// this example uses the native Raspberry Pi GPIO driver

using namespace el1::dev::gpio;
using namespace el1::dev::gpio::bcm283x;

int main(int argc, char* argv[])
{
	try
	{
		if(argc != 5)
			throw "need exactly four integer arguments (pin# | delay in ms | repeat count | pull=0/drive=1)";

		const int gpio_num = atoi(argv[1]);
		const int delay_ms = atoi(argv[2]);
		const int n_repeat = atoi(argv[3]);
		const int mode = atoi(argv[4]);
		TBCM283X& gpio_controller = *TBCM283X::Instance();

		auto gpio_pin = gpio_controller.ClaimPin(gpio_num);

		if(mode == 0)
		{
			gpio_pin->Mode(EMode::INPUT);

			for(unsigned i = 0; i < n_repeat; i++)
			{
				gpio_pin->Pull(EPull::DOWN);
				usleep(delay_ms * 1000UL);
				gpio_pin->Pull(EPull::UP);
				usleep(delay_ms * 1000UL);
			}
		}
		else if(mode == 1)
		{
			gpio_pin->Mode(EMode::OUTPUT);

			for(unsigned i = 0; i < n_repeat; i++)
			{
				gpio_pin->State(false);
				usleep(delay_ms * 1000UL);
				gpio_pin->State(true);
				usleep(delay_ms * 1000UL);
			}
		}

		return 0;
	}
	catch(const TException& e)
	{
		std::cerr<<"ERROR: "<<e.what()<<"\n";
	}
	catch(const char* const msg)
	{
		std::cerr<<"ERROR: "<<msg<<"\n";
	}

	return 1;
}
