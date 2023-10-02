#include "dev_i2c_ads111x.hpp"
#include <string.h>

namespace el1::dev::i2c::ads111x
{
	void config_t::OpMode(const EOpMode om)
	{
		switch(om)
		{
			case EOpMode::CONTINUOUS:
				mode = 0;
				return;
			case EOpMode::SINGLE_SHOT:
				mode = 1;
				return;
		}
		EL_THROW(TLogicException);
	}

	EOpMode config_t::OpMode() const
	{
		return mode ? EOpMode::SINGLE_SHOT : EOpMode::CONTINUOUS;
	}

	void config_t::PGA(const EPGA p)
	{
		pga = (u16_t)p;
	}

	EPGA config_t::PGA() const
	{
		return (EPGA)pga;
	}

	void config_t::Channel(const EChannel ch)
	{
		mux = (u16_t)ch;
	}

	EChannel config_t::Channel() const
	{
		return (EChannel)mux;
	}

	void config_t::DataRate(const EDataRate rate)
	{
		dr = (u16_t)rate;
	}

	EDataRate config_t::DataRate() const
	{
		return (EDataRate)dr;
	}

	void config_t::DisableIrq()
	{
		comp_que = 3;
	}

	void config_t::ConfigureDataReadyIrq()
	{
		comp_mode = 0;
		comp_lat = 1;
		comp_que = 0;
		comp_low  = -32768;
		comp_high = -32768;
	}

	void config_t::ConfigureWindowIrq(const s16_t low, const s16_t high, const ECompQueue q)
	{
		comp_mode = 1;
		comp_lat = 1;
		comp_que = (u16_t)q;
		comp_low  = low;
		comp_high = high;
	}

	void config_t::ConfigureHysteresisIrq(const s16_t low, const s16_t high, const ECompQueue q)
	{
		comp_mode = 0;
		comp_lat = 0;
		comp_que = (u16_t)q;
		comp_low  = low;
		comp_high = high;
	}

	config_t::config_t(const config_t& rhs)
	{
		_word = rhs._word;
		comp_low = rhs.comp_low;
		comp_high = rhs.comp_high;
		os = 0;
	}

	config_t::config_t()
	{
		_word = 0x8583;
		comp_low = 0x8000;
		comp_high = 0x7FFF;
	}

	void TADS111X::ReadConfig()
	{
		current_config._word = device->ReadWordRegister(1);
		current_config.comp_low = device->ReadWordRegister(2);
		current_config.comp_high = device->ReadWordRegister(3);
	}

	void TADS111X::WriteConfig(const config_t new_config)
	{
		if(new_config._word != current_config._word)
		{
			device->WriteWordRegister(1, new_config._word);
			current_config._word = new_config._word;
		}

		if(new_config.comp_low != current_config.comp_low)
		{
			device->WriteWordRegister(2, new_config.comp_low);
			current_config.comp_low = new_config.comp_low;
		}

		if(new_config.comp_high != current_config.comp_high)
		{
			device->WriteWordRegister(3, new_config.comp_high);
			current_config.comp_high = new_config.comp_high;
		}
	}

	usys_t TADS111X::Read(s16_t* const arr_items, const usys_t n_items_max)
	{
		if(n_items_max == 0 || irq->State())
			return 0;

		arr_items[0] = (s16_t)device->ReadWordRegister(0);
		return 1;
	}

	const system::waitable::IWaitable* TADS111X::OnInputReady() const
	{
		return &irq->OnInputTrigger();
	}

	void TADS111X::Config(const config_t new_config)
	{
		WriteConfig(new_config);
	}

	const config_t& TADS111X::Config() const
	{
		return current_config;
	}

	void TADS111X::TriggerConversion()
	{
		current_config.os = 1;
		device->WriteWordRegister(1, current_config._word);
	}

	bool TADS111X::Busy() const
	{
		config_t c;
		c._word = device->ReadWordRegister(1);
		return c.os == 0;
	}

	TADS111X::TADS111X(std::unique_ptr<II2CDevice> device, std::unique_ptr<gpio::IPin> irq) : device(std::move(device)), irq(std::move(irq))
	{
		ReadConfig();
	}
}
