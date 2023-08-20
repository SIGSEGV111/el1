#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"

namespace el1::dev::i2c
{
	using namespace io::types;

	struct II2CDevice;
	struct II2CBus;

	enum class ESpeedClass : u8_t
	{
		STD  = 1,	//	100 KBit/s
		FULL = 2,	//	400 KBit/s
		FAST = 3,	//	1.0 MBit/s
		HIGH = 4	//	3.2 MBit/s
	};

	struct II2CDevice : io::stream::ISource<byte_t>, io::stream::ISink<byte_t>
	{
		virtual ~II2CDevice() {}
		virtual II2CBus* Bus() const EL_GETTER = 0;
		virtual u8_t Address() const EL_GETTER = 0;
		virtual ESpeedClass SpeedClass() const EL_GETTER = 0;
	};

	struct II2CBus
	{
		virtual ESpeedClass MaxSupportedSpeed() const EL_GETTER = 0;
		virtual std::unique_ptr<II2CDevice> ClaimDevice(const u8_t address, const ESpeedClass sc_max = ESpeedClass::STD) = 0;
	};
}
