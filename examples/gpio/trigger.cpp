#include <iostream>
#include <unistd.h>
#include "../../gen/dbg/amalgam/el1.cpp"

using namespace el1::dev::gpio;
using namespace el1::dev::gpio::sysfs;

int main(int argc, char* argv[])
{
	try
	{
		if(argc != 4)
			throw "need exactly three integer arguments (pin# | timeout in ms | repeat count)";

		const int gpio_num = atoi(argv[1]);
		const int max_wait_ms = atoi(argv[2]);
		const int n_repeat = atoi(argv[3]);
		TController& gpio_controller = *TController::Instance();

		auto gpio_pin = gpio_controller.ClaimPin(gpio_num);
		gpio_pin->Mode(EMode::INPUT);
		gpio_pin->Trigger(ETrigger::BOTH_EDGES);
		std::cerr<<"state = "<<gpio_pin->State()<<"\n";

		for(unsigned i = 0; i < n_repeat; i++)
		{
			std::cerr<<"waiting... ";
			if(gpio_pin->OnInputTrigger().WaitFor(max_wait_ms / 1000.0))
			{
				std::cerr<<"TRIGGERED!\nstate = "<<gpio_pin->State()<<"\n";
			}
			else
			{
				std::cerr<<"TIMEOUT!\n";
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
