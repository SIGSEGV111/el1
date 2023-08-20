#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"

namespace el1::dev::i2c
{
	using namespace io::types;

	struct II2CDevice;
	struct II2CBus;

	enum class ESpeedClass : u32_t
	{
		STD  =  100000,	//	100 KBit/s
		FULL =  400000,	//	400 KBit/s
		FAST = 1000000,	//	1.0 MBit/s
		HIGH = 3200000	//	3.2 MBit/s
	};

	struct II2CDevice : io::stream::ISource<byte_t>, io::stream::ISink<byte_t>
	{
		virtual ~II2CDevice() {}
		virtual II2CBus* Bus() const EL_GETTER = 0;
		virtual u8_t Address() const EL_GETTER = 0;
		virtual ESpeedClass SpeedClass() const EL_GETTER = 0;

		u8_t  ReadByteRegister(const u8_t regaddr);
		u16_t ReadWordRegister(const u8_t regaddr);
		u32_t ReadLongRegister(const u8_t regaddr);
		void  ReadRegisterArray(const u8_t regaddr_start, const u8_t n_bytes, byte_t* const arr_bytes);

		void WriteByteRegister(const u8_t regaddr, const u8_t  value);
		void WriteWordRegister(const u8_t regaddr, const u16_t value);
		void WriteLongRegister(const u8_t regaddr, const u32_t value);
		void WriteRegisterArray(const u8_t regaddr_start, const u8_t n_bytes, const byte_t* const arr_bytes);
	};

	struct II2CBus
	{
		virtual ESpeedClass MaxSupportedSpeed() const EL_GETTER = 0;
		virtual std::unique_ptr<II2CDevice> ClaimDevice(const u8_t address, const ESpeedClass sc_max = ESpeedClass::STD) = 0;
	};
}
