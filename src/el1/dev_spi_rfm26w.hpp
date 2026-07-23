#pragma once

#include "dev_spi_si446x.hpp"

namespace el1::dev::spi::rfm26w
{
	using si446x::ECommand;
	using si446x::TConfiguration;
	using si446x::TDeviceState;
	using si446x::TFifoInfo;
	using si446x::TInterruptStatus;
	using si446x::TPartInfo;
	using TRFM26W = si446x::TRadio;
}
