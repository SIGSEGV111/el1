#pragma once

#include "def.hpp"
#include "dev_i2c.hpp"

namespace el1::dev::i2c::ina219
{
	enum struct EBusVoltageRange : u8_t
	{
		V16 = 0,
		V32 = 1,
	};

	enum struct EShuntVoltageRange : u8_t
	{
		MV40  = 0,
		MV80  = 1,
		MV160 = 2,
		MV320 = 3,
	};

	enum struct EADCResolution : u8_t
	{
		BIT_9   = 0x0,
		BIT_10  = 0x1,
		BIT_11  = 0x2,
		BIT_12  = 0x3,
		AVG_2   = 0x9,
		AVG_4   = 0xA,
		AVG_8   = 0xB,
		AVG_16  = 0xC,
		AVG_32  = 0xD,
		AVG_64  = 0xE,
		AVG_128 = 0xF,
	};

	enum struct EMode : u8_t
	{
		POWER_DOWN                 = 0,
		SHUNT_TRIGGERED            = 1,
		BUS_TRIGGERED              = 2,
		SHUNT_AND_BUS_TRIGGERED    = 3,
		ADC_DISABLED               = 4,
		SHUNT_CONTINUOUS           = 5,
		BUS_CONTINUOUS             = 6,
		SHUNT_AND_BUS_CONTINUOUS   = 7,
	};

	struct config_t
	{
		EBusVoltageRange bus_voltage_range;
		EShuntVoltageRange shunt_voltage_range;
		EADCResolution bus_adc_resolution;
		EADCResolution shunt_adc_resolution;
		EMode mode;

		u16_t Word() const EL_GETTER;
		static config_t FromWord(const u16_t word);

		config_t();
	};

	struct measurement_t
	{
		double shunt_voltage;
		double bus_voltage;
		double current;
		bool conversion_ready;
		bool math_overflow;
	};

	class TINA219
	{
		protected:
			std::unique_ptr<II2CDevice> device;
			const double shunt_resistance;
			double current_lsb;

			u16_t ReadBusVoltageRegister();

		public:
			void Reset();

			config_t Config();
			void Config(const config_t& config);

			// The shunt-voltage register has a fixed 10uV LSB.
			double ShuntVoltage();

			// The bus-voltage register has a fixed 4mV LSB.
			double BusVoltage();

			// Computes current directly from the measured shunt voltage and the configured shunt resistance.
			// This does not require programming the INA219 calibration register.
			double Current();

			// Programs the calibration register for approximately the requested Current Register LSB.
			// CurrentLsb() returns the exact LSB resulting from the programmed integer calibration value.
			void Calibrate(const double requested_current_lsb);
			void CalibrateForMaxCurrent(const double max_expected_current);
			double CurrentLsb() const EL_GETTER;
			double CalibratedCurrent();
			double Power();

			bool ConversionReady();
			bool MathOverflow();
			measurement_t Measure();

			double ShuntResistance() const EL_GETTER { return shunt_resistance; }

			TINA219(std::unique_ptr<II2CDevice> device, const double shunt_resistance);
	};
}
