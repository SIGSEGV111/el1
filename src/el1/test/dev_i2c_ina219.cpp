#include <gtest/gtest.h>
#include <el1/dev_i2c_ina219.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::dev::i2c::ina219;

	TEST(dev_i2c_ina219, config_default_matches_power_on_value)
	{
		const config_t config;
		EXPECT_EQ(config.Word(), 0x399F);
	}

	TEST(dev_i2c_ina219, config_roundtrip)
	{
		config_t config;
		config.bus_voltage_range = EBusVoltageRange::V16;
		config.shunt_voltage_range = EShuntVoltageRange::MV80;
		config.bus_adc_resolution = EADCResolution::AVG_16;
		config.shunt_adc_resolution = EADCResolution::AVG_64;
		config.mode = EMode::SHUNT_CONTINUOUS;

		const config_t decoded = config_t::FromWord(config.Word());
		EXPECT_EQ(decoded.bus_voltage_range, config.bus_voltage_range);
		EXPECT_EQ(decoded.shunt_voltage_range, config.shunt_voltage_range);
		EXPECT_EQ(decoded.bus_adc_resolution, config.bus_adc_resolution);
		EXPECT_EQ(decoded.shunt_adc_resolution, config.shunt_adc_resolution);
		EXPECT_EQ(decoded.mode, config.mode);
	}
}
