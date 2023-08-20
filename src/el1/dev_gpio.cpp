#include "dev_gpio.hpp"
#include "error.hpp"

namespace el1::dev::gpio
{
	void IPin::Commit()
	{
		this->Controller()->Commit();
	}

	IPin::~IPin()
	{
	}
}
