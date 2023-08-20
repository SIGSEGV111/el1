#pragma once

#include "dev_w1.hpp"
#include "system_time_timer.hpp"

namespace el1::dev::w1::ds18x20
{
	using namespace io::types;

	enum class EModel : byte_t
	{
		DS18B20 = 0x28,
		DS18S20 = 0x10
	};

	struct scratchpad_ds18s20_t
	{
		u8_t temp_raw;
		u8_t sign;
		u8_t th;
		u8_t tl;
		u8_t __reserved1;
		u8_t __reserved2;
		u8_t count_remain;
		u8_t count_per_degc;
		u8_t crc;
	} EL_PACKED;

	struct scratchpad_ds18b20_t
	{
		u16_t temp_raw;
		u8_t th;
		u8_t tl;
		u8_t config;
		u8_t __reserved1;
		u8_t __reserved2;
		u8_t __reserved3;
		u8_t crc;
	} EL_PACKED;

	static const byte_t CMD_POWER_STATE = 0xB4;
	static const byte_t CMD_READ_SCRATCHPAD = 0xBE;
	static const byte_t CMD_WRITE_SCRATCHPAD = 0x4E;
	static const byte_t CMD_TRIGGER_CONVERSION = 0x44;

	class TDS18X20
	{
		protected:
			std::unique_ptr<IW1Device> w1dev;
			const u8_t bus_powered;

			union
			{
				struct
				{
					u8_t temp_raw;
					u8_t count_remain;
					u8_t count_per_degc;
				} ds18s20;

				struct
				{
					u16_t temp_raw;
				} ds18b20;
			};

			// true = bus powered, false = dedicated power supply
			bool DetectBusPowered();

		public:
			static io::text::string::TString ModelName(const EModel);

			EModel Model() const EL_GETTER;
			io::text::string::TString ModelName() const EL_GETTER;
			const IW1Device& Device() const EL_GETTER { return *w1dev; }

			// returns the temperature in °C
			float Temperature() const EL_GETTER;

			// instruct the sensor to start a new measurment (this might cancel and restart any ongoing measurment)
			// a measurment will take at most 750ms - this function will not block, it merly sends the command to the sensor
			void TriggerConversion();

			// fetches the result of the measurment from sensor into local cache, might return garbage if the sensor is still converting
			void Fetch();

			// all in one utility-function: TriggerConversion() + blocking wait for 750ms + Fetch() + return Temperature()
			float Poll();

			// if power_source is PARASITIC, then the bus must be paused during conversion to provide enough power to the sensor
			// also a strong pullup must be provided by the bus
			TDS18X20(std::unique_ptr<IW1Device> w1dev, const EPowerSource power_source = EPowerSource::AUTO_DETECT);
	};
}
