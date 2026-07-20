#include "dev_i2c_ina219.hpp"
#include "error.hpp"
#include <math.h>

namespace el1::dev::i2c::ina219
{
	static const u8_t REG_CONFIG        = 0x00;
	static const u8_t REG_SHUNT_VOLTAGE = 0x01;
	static const u8_t REG_BUS_VOLTAGE   = 0x02;
	static const u8_t REG_POWER         = 0x03;
	static const u8_t REG_CURRENT       = 0x04;
	static const u8_t REG_CALIBRATION   = 0x05;

	static const double SHUNT_VOLTAGE_LSB = 10e-6;
	static const double BUS_VOLTAGE_LSB = 4e-3;
	static const double CALIBRATION_SCALE = 0.04096;

	u16_t config_t::Word() const
	{
		return
			(static_cast<u16_t>(bus_voltage_range) << 13) |
			(static_cast<u16_t>(shunt_voltage_range) << 11) |
			(static_cast<u16_t>(bus_adc_resolution) << 7) |
			(static_cast<u16_t>(shunt_adc_resolution) << 3) |
			static_cast<u16_t>(mode);
	}

	config_t config_t::FromWord(const u16_t word)
	{
		config_t config;
		config.bus_voltage_range = static_cast<EBusVoltageRange>((word >> 13) & 0x01);
		config.shunt_voltage_range = static_cast<EShuntVoltageRange>((word >> 11) & 0x03);
		config.bus_adc_resolution = static_cast<EADCResolution>((word >> 7) & 0x0F);
		config.shunt_adc_resolution = static_cast<EADCResolution>((word >> 3) & 0x0F);
		config.mode = static_cast<EMode>(word & 0x07);
		return config;
	}

	config_t::config_t() :
		bus_voltage_range(EBusVoltageRange::V32),
		shunt_voltage_range(EShuntVoltageRange::MV320),
		bus_adc_resolution(EADCResolution::BIT_12),
		shunt_adc_resolution(EADCResolution::BIT_12),
		mode(EMode::SHUNT_AND_BUS_CONTINUOUS)
	{
	}

	u16_t TINA219::ReadBusVoltageRegister()
	{
		return device->ReadWordRegister(REG_BUS_VOLTAGE);
	}

	void TINA219::Reset()
	{
		device->WriteWordRegister(REG_CONFIG, 0x8000);
		current_lsb = NAN;
	}

	config_t TINA219::Config()
	{
		return config_t::FromWord(device->ReadWordRegister(REG_CONFIG));
	}

	void TINA219::Config(const config_t& config)
	{
		device->WriteWordRegister(REG_CONFIG, config.Word());
	}

	double TINA219::ShuntVoltage()
	{
		return static_cast<s16_t>(device->ReadWordRegister(REG_SHUNT_VOLTAGE)) * SHUNT_VOLTAGE_LSB;
	}

	double TINA219::BusVoltage()
	{
		return static_cast<double>(ReadBusVoltageRegister() >> 3) * BUS_VOLTAGE_LSB;
	}

	double TINA219::Current()
	{
		return ShuntVoltage() / shunt_resistance;
	}

	void TINA219::Calibrate(const double requested_current_lsb)
	{
		EL_ERROR(!isfinite(requested_current_lsb) || requested_current_lsb <= 0.0, TInvalidArgumentException, "requested_current_lsb", "current LSB must be finite and greater than zero");

		const double calibration_f = CALIBRATION_SCALE / (requested_current_lsb * shunt_resistance);
		EL_ERROR(!isfinite(calibration_f) || calibration_f < 2.0 || calibration_f > 65535.0, TInvalidArgumentException, "requested_current_lsb", "requested current LSB results in an out-of-range calibration value");

		// FS0 is a void bit and always reads as zero. Keep it clear explicitly so CurrentLsb()
		// describes the exact integer calibration value used by the device.
		const u16_t calibration = static_cast<u16_t>(calibration_f) & 0xFFFE;
		EL_ERROR(calibration == 0, TInvalidArgumentException, "requested_current_lsb", "requested current LSB results in a zero calibration value");

		device->WriteWordRegister(REG_CALIBRATION, calibration);
		current_lsb = CALIBRATION_SCALE / (static_cast<double>(calibration) * shunt_resistance);
	}

	void TINA219::CalibrateForMaxCurrent(const double max_expected_current)
	{
		EL_ERROR(!isfinite(max_expected_current) || max_expected_current <= 0.0, TInvalidArgumentException, "max_expected_current", "maximum expected current must be finite and greater than zero");
		Calibrate(max_expected_current / 32768.0);
	}

	double TINA219::CurrentLsb() const
	{
		return current_lsb;
	}

	double TINA219::CalibratedCurrent()
	{
		EL_ERROR(!isfinite(current_lsb), TLogicException);
		return static_cast<s16_t>(device->ReadWordRegister(REG_CURRENT)) * current_lsb;
	}

	double TINA219::Power()
	{
		EL_ERROR(!isfinite(current_lsb), TLogicException);
		return device->ReadWordRegister(REG_POWER) * current_lsb * 20.0;
	}

	bool TINA219::ConversionReady()
	{
		return (ReadBusVoltageRegister() & 0x0002) != 0;
	}

	bool TINA219::MathOverflow()
	{
		return (ReadBusVoltageRegister() & 0x0001) != 0;
	}

	measurement_t TINA219::Measure()
	{
		const double shunt_voltage = ShuntVoltage();
		const u16_t bus_voltage_register = ReadBusVoltageRegister();
		return {
			shunt_voltage,
			static_cast<double>(bus_voltage_register >> 3) * BUS_VOLTAGE_LSB,
			shunt_voltage / shunt_resistance,
			(bus_voltage_register & 0x0002) != 0,
			(bus_voltage_register & 0x0001) != 0
		};
	}

	TINA219::TINA219(std::unique_ptr<II2CDevice> _device, const double _shunt_resistance) :
		device(std::move(_device)),
		shunt_resistance(_shunt_resistance),
		current_lsb(NAN)
	{
		EL_ERROR(device == nullptr, TInvalidArgumentException, "device", "I2C device must not be null");
		EL_ERROR(!isfinite(shunt_resistance) || shunt_resistance <= 0.0, TInvalidArgumentException, "shunt_resistance", "shunt resistance must be finite and greater than zero");
		device->cache_addr = true;
	}
}
