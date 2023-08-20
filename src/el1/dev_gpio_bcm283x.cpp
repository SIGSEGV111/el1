#include "dev_gpio_bcm283x.hpp"

#ifdef EL_OS_LINUX

namespace el1::dev::gpio::bcm283x
{
	// see `cat /sys/firmware/devicetree/base/model; echo`
	// addresses and cpu found by `dmesg | grep gpio`
	static const model_config_t MODEL_CONFIG[] = {
		{ "Raspberry Pi Model B Plus Rev 1.2",   0x20200000, ECPU::BCM283X },
		{ "Raspberry Pi Model A Plus Rev 1.1",   0x20200000, ECPU::BCM283X },
		{ "Raspberry Pi Compute Module Rev 1.0", 0x20200000, ECPU::BCM283X },
		{ "Raspberry Pi Zero W Rev 1.1",         0x20200000, ECPU::BCM283X },
		{ "Raspberry Pi 2 Model B Rev 1.1",      0x3f200000, ECPU::BCM283X },
		{ "Raspberry Pi 3 Model B Plus Rev 1.3", 0x3f200000, ECPU::BCM283X },
		{ "Raspberry Pi Zero 2 W Rev 1.0",       0x3f200000, ECPU::BCM283X },
		{ "Raspberry Pi 4 Model B Rev 1.1",      0xfe200000, ECPU::BCM2711 },
	};

	static const u32_t GPIO_SIZE = 4096;

	static const u32_t GPFSEL_OFFSET = 0;
	static const u32_t GPSET_OFFSET = 0x1C / 4;
	static const u32_t GPCLR_OFFSET = 0x28 / 4;
	static const u32_t GPLEV_OFFSET = 0x34 / 4;
	static const u32_t BCM283X_GPPUD_OFFSET = 0x94 / 4;
	static const u32_t BCM283X_GPPUDCLK_OFFSET = 0x98 / 4;
	static const u32_t BCM2711_GPPUD_OFFSET = 0xE4 / 4;

	static volatile u32_t* gpio = nullptr;

	static void Delay(const unsigned n_clock_cycles)
	{
		// below code is ugly, but compiler would complain otherwise
		volatile unsigned x = 0;
		while(x < n_clock_cycles)
		{
			unsigned y = x;
			y++;
			x = y;
		}
	}

	static u32_t MakeMask(const u8_t n_bits, const u8_t shift)
	{
		if(n_bits == 0)
			return 0;

		if(n_bits == 32)
			return 0xffffffff;

		u32_t mask = 1 << n_bits;
		mask--;
		mask <<= shift;

		return mask;
	}

	static u32_t GetBits(const u32_t reg, const u8_t n_bits, const u8_t at)
	{
		const u32_t mask = MakeMask(n_bits, at);
		return (reg & mask) >> at;
	}

	static void SetBits(volatile u32_t& reg, const u8_t n_bits, const u8_t at, u32_t value)
	{
		const u32_t mask = MakeMask(n_bits, at);
		value <<= at;
		value &= mask;

		u32_t tmp = reg & (~mask);
		tmp |= value;
		reg = tmp;
	}

	IController* TPin::Controller() const
	{
		return TBCM283X::Instance();
	}

	void TPin::AutoCommit(const bool)
	{
		// noting to do
		// auto-commit is always on
	}

	bool TPin::State() const
	{
		volatile u32_t& reg = gpio[GPLEV_OFFSET + index / 32];
		return (reg & (1 << (index % 32))) != 0;
	}

	void TPin::State(const bool new_state)
	{
		volatile u32_t& reg = gpio[(new_state ? GPSET_OFFSET : GPCLR_OFFSET) + index / 32];
		reg = reg | (1 << (index % 32));
	}

	EMode TPin::Mode() const
	{
		volatile u32_t& reg = gpio[GPFSEL_OFFSET + index / 10];
		const u32_t mode_bits = GetBits(reg, 3, (index % 10) * 3);

		switch(mode_bits)
		{
			case 0b000:
				return EMode::INPUT;

			case 0b001:
				return EMode::OUTPUT;

			case 0b100:
			case 0b101:
			case 0b110:
			case 0b111:
			case 0b011:
			case 0b010:
				return EMode::ALT_FUNC;

			default:
				EL_THROW(TLogicException);
		}
	}

	void TPin::Mode(const EMode new_mode)
	{
		volatile u32_t& reg = gpio[GPFSEL_OFFSET + index / 10];

		switch(new_mode)
		{
			case EMode::DISABLED:
			case EMode::INPUT:
				SetBits(reg, 3, (index % 10) * 3, 0b000);
				return;

			case EMode::OUTPUT:
				SetBits(reg, 3, (index % 10) * 3, 0b001);
				return;

			case EMode::ALT_FUNC:
				EL_THROW(TInvalidArgumentException, "new_mode", "ALT_FUNC can only be set through AlternateFunction(...)");
		}

		EL_THROW(TLogicException);
	}

	void TPin::AlternateFunction(const int func)
	{
		volatile u32_t& reg = gpio[GPFSEL_OFFSET + index / 10];

		switch(func)
		{
			case 0: SetBits(reg, 3, (index % 10) * 3, 0b100); break;
			case 1: SetBits(reg, 3, (index % 10) * 3, 0b101); break;
			case 2: SetBits(reg, 3, (index % 10) * 3, 0b110); break;
			case 3: SetBits(reg, 3, (index % 10) * 3, 0b111); break;
			case 4: SetBits(reg, 3, (index % 10) * 3, 0b011); break;
			case 5: SetBits(reg, 3, (index % 10) * 3, 0b010); break;
			default: EL_THROW(TInvalidArgumentException, "func", "func must be in the range 0-5");
		}
	}

	int TPin::AlternateFunction() const
	{
		volatile u32_t& reg = gpio[GPFSEL_OFFSET + index / 10];
		const u32_t mode_bits = GetBits(reg, 3, (index % 10) * 3);

		switch(mode_bits)
		{
			case 0b000: return -1;
			case 0b001: return -1;
			case 0b100: return 0;
			case 0b101: return 1;
			case 0b110: return 2;
			case 0b111: return 3;
			case 0b011: return 4;
			case 0b010: return 5;
			default: EL_THROW(TLogicException);
		}
	}

	void TPin::Pull(const EPull new_pull)
	{
		if(pull == new_pull) return;
		controller->_PullResistor(index, new_pull);
		pull = new_pull;
	}

	ETrigger TPin::Trigger() const
	{
		EL_NOT_IMPLEMENTED;
	}

	void TPin::Trigger(const ETrigger)
	{
		EL_NOT_IMPLEMENTED;
	}

	EPull TPin::Pull() const
	{
		return pull;
	}

	THandleWaitable& TPin::OnInputTrigger()
	{
		EL_NOT_IMPLEMENTED;
	}

	TPin::TPin(TBCM283X* const controller, const u8_t index) : controller(controller), index(index), pull(EPull::UP)
	{
		EL_ERROR(index >= NUM_GPIO, TInvalidArgumentException, "index", "index must be < NUM_GPIO");
		EL_ERROR( (controller->claimed_pins & (1<<index)) != 0, TException, TString::Format("GPIO pin #%d is already in use", index));
		controller->claimed_pins |= (1<<index);
		Mode(EMode::INPUT);
		Pull(EPull::DISABLED);
	}

	TPin::~TPin()
	{
		Mode(EMode::INPUT);
		Pull(EPull::DISABLED);
		controller->claimed_pins &= ~(1<<index);
	}

	static const model_config_t* DetectModel()
	{
		const char buffer[64] = {};
		TFile model_file("/sys/firmware/devicetree/base/model");
		model_file.Read((byte_t*)buffer, sizeof(buffer) - 1);

		for(unsigned i = 0; i < sizeof(MODEL_CONFIG)/sizeof(MODEL_CONFIG[0]); i++)
			if(strcmp(MODEL_CONFIG[i].name, buffer) == 0)
				return MODEL_CONFIG + i;

		EL_THROW(TException, TString::Format("unknown Raspberry Pi Model: %q; please update MODEL_CONFIG array", buffer));
	}

	TBCM283X::TBCM283X() :
		claimed_pins(0),
		model(DetectModel()),
		file("/dev/mem", TAccess::RW),
		map(&file, model->addr, GPIO_SIZE)
	{
		gpio = (volatile u32_t*)&map[0];
	}

	void TBCM283X::_PullResistor(const unsigned index, const EPull new_pull)
	{
		if(model->cpu == ECPU::BCM283X)
		{
			volatile u32_t& reg = gpio[BCM283X_GPPUDCLK_OFFSET + index / 32];

			switch(new_pull)
			{
				case EPull::DISABLED:
					gpio[BCM283X_GPPUD_OFFSET] = 0b00;
					break;

				case EPull::UP:
					gpio[BCM283X_GPPUD_OFFSET] = 0b10;
					break;

				case EPull::DOWN:
					gpio[BCM283X_GPPUD_OFFSET] = 0b01;
					break;

				default:
					EL_THROW(TLogicException);
			}

			Delay(150);
			reg = (1 << (index % 32));
			Delay(150);
			gpio[BCM283X_GPPUD_OFFSET] = 0b00;
			reg = 0;
		}
		else if(model->cpu == ECPU::BCM2711)
		{
			volatile u32_t& reg = gpio[BCM2711_GPPUD_OFFSET + index / 16];

			switch(new_pull)
			{
				case EPull::DISABLED:
					SetBits(reg, 2, (index % 16) * 2, 0b00);
					break;

				case EPull::UP:
					SetBits(reg, 2, (index % 16) * 2, 0b01);
					break;

				case EPull::DOWN:
					SetBits(reg, 2, (index % 16) * 2, 0b10);
					break;

				default:
					EL_THROW(TLogicException);
			}
		}
		else
			EL_THROW(TLogicException);
	}

	std::unique_ptr<IPin> TBCM283X::ClaimPin(const usys_t index)
	{
		return std::unique_ptr<IPin>(new TPin(this, (u8_t)index));
	}

	TBCM283X* TBCM283X::Instance()
	{
		static std::unique_ptr<TBCM283X> instance = std::unique_ptr<TBCM283X>(new TBCM283X());
		return instance.get();
	}

}

#endif
