#include "dev_i2c_ads111x.hpp"
#include <string.h>

namespace el1::dev::i2c::ads111x
{
	bool DEBUG = false;
	using namespace system::time;
	using namespace system::time::timer;

	#define IF_DEBUG if(EL_UNLIKELY(DEBUG))

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

	void config_t::ConfigureWindowComparator(const s16_t low, const s16_t high, const ECompQueue q)
	{
		comp_mode = 1;
		comp_lat = 0;
		comp_que = (u16_t)q;
		comp_low  = low;
		comp_high = high;
	}

	void config_t::ConfigureHysteresisComparator(const s16_t low, const s16_t high, const ECompQueue q)
	{
		comp_mode = 0;
		comp_lat = 0;
		comp_que = (u16_t)q;
		comp_low  = low;
		comp_high = high;
	}

	bool config_t::HasIrq() const
	{
		return comp_mode == 0 && comp_lat == 1 && comp_que == 0 && comp_low == -32768 && comp_high == -32768;
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
		IF_DEBUG fprintf(stderr, "TADS111X::ReadConfig(): ... ");
		current_config._word = device->ReadWordRegister(1);
		current_config.comp_low = device->ReadWordRegister(2);
		current_config.comp_high = device->ReadWordRegister(3);
		current_config.os = 0;
		IF_DEBUG fprintf(stderr, "config = 0x%04hx, low = %hd, high = %hd\n", current_config._word, current_config.comp_low, current_config.comp_high);
	}

	void TADS111X::WriteConfig(const config_t new_config)
	{
		IF_DEBUG fprintf(stderr, "TADS111X::WriteConfig(): config = 0x%04hx, low = %hd, high = %hd ... ", new_config._word, new_config.comp_low, new_config.comp_high);

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
		IF_DEBUG fprintf(stderr, "done\n");
	}

	usys_t TADS111X::Read(s16_t* const arr_items, const usys_t n_items_max)
	{
		if(n_items_max == 0)
			return 0;

		if(current_config.HasIrq() && irq != nullptr && irq->State())
		{
			IF_DEBUG fprintf(stderr, "TADS111X::Read(): IRQ not signaled => return 0\n");
			return 0;
		}

		if(current_config.OpMode() == EOpMode::SINGLE_SHOT && Busy())
		{
			IF_DEBUG fprintf(stderr, "TADS111X::Read(): chip still busy => return 0\n");
			return 0;
		}

		// use timer and pray
		if(!current_config.HasIrq() && current_config.OpMode() == EOpMode::CONTINUOUS && (timer == nullptr || timer->ReadMissedTicksCount() == 0))
			return 0;

		arr_items[0] = (s16_t)device->ReadWordRegister(0);
		IF_DEBUG fprintf(stderr, "TADS111X::Read(): read result %hd => return 1\n", arr_items[0]);
		return 1;
	}

	const system::waitable::IWaitable* TADS111X::OnInputReady() const
	{
		if(current_config.HasIrq() && irq != nullptr)
			return &irq->OnInputTrigger();
		else if(!current_config.HasIrq() && timer != nullptr)
			return &timer->OnTick();
		else
			return nullptr;
	}

	static TTime ComputeSampleInterval(const EDataRate dr)
	{
		EL_ERROR((u8_t)dr >= (sizeof(ARR_DATARATE) / sizeof(ARR_DATARATE[0])), TInvalidArgumentException, "dr", "dr out of range");
		return TTime(1.0 / (double)ARR_DATARATE[(u8_t)dr]);
	}

	void TADS111X::Config(const config_t new_config)
	{
		if(irq != nullptr)
		{
			if(new_config.HasIrq())
			{
				IF_DEBUG fprintf(stderr, "TADS111X::Config(): setting up IRQ ... ");
				irq->Mode(gpio::EMode::INPUT);
				irq->Trigger(gpio::ETrigger::FALLING_EDGE);
				irq->State(); // clear any pending IRQ
				IF_DEBUG fprintf(stderr, "done\n");
			}
			else
			{
				IF_DEBUG fprintf(stderr, "TADS111X::Config(): disabling IRQ ... ");
				irq->Mode(gpio::EMode::DISABLED);
				irq->Trigger(gpio::ETrigger::DISABLED);
				IF_DEBUG fprintf(stderr, "done\n");
			}
		}

		device->ReadWordRegister(0);	// resets the chips IRQ circuit
		WriteConfig(new_config);

		if(!new_config.HasIrq())
		{
			if(timer == nullptr)
				timer = std::unique_ptr<TTimer>(new TTimer(EClock::MONOTONIC, ComputeSampleInterval((EDataRate)new_config.dr)));
			else
				timer->Start(ComputeSampleInterval((EDataRate)new_config.dr));

			timer->ReadMissedTicksCount(); // reset timer IRQ
		}
	}

	const config_t& TADS111X::Config() const
	{
		return current_config;
	}

	void TADS111X::TriggerConversion()
	{
		IF_DEBUG fprintf(stderr, "TADS111X::TriggerConversion(): triggering conversion ... ");
		current_config.os = 1;
		device->WriteWordRegister(1, current_config._word);
		current_config.os = 0;
		IF_DEBUG fprintf(stderr, "done\n");
	}

	bool TADS111X::Busy() const
	{
		IF_DEBUG fprintf(stderr, "TADS111X::Busy(): checking busy ... ");
		config_t c;
		c._word = device->ReadWordRegister(1);
		IF_DEBUG fprintf(stderr, "%s\n", c.os == 0 ? "busy" : "idle");
		return c.os == 0;
	}

	TADS111X::TADS111X(std::unique_ptr<II2CDevice> device, std::unique_ptr<gpio::IPin> irq) : device(std::move(device)), irq(std::move(irq))
	{
		this->device->cache_addr = true;
		ReadConfig();
	}

	#undef IF_DEBUG
}
