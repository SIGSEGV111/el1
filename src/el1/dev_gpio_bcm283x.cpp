#include "dev_gpio_bcm283x.hpp"

#ifdef EL_OS_LINUX

namespace el1::dev::gpio::bcm283x
{
	// see '/sys/firmware/devicetree/base/model'
	const model_config_t MODEL_CONFIG[] = {
		{ "Raspberry Pi Model B Rev 1.0",			0x20000000,	ESOC::BCM283X, ECPU::ARM1176JZF_S },
		{ "Raspberry Pi Model B Rev 2.0",			0x20000000,	ESOC::BCM283X, ECPU::ARM1176JZF_S },
		{ "Raspberry Pi Model A",					0x20000000,	ESOC::BCM283X, ECPU::ARM1176JZF_S },
		{ "Raspberry Pi Model B Plus Rev 1.2",		0x20000000,	ESOC::BCM283X, ECPU::ARM1176JZF_S },
		{ "Raspberry Pi Model A Plus Rev 1.1",		0x20000000,	ESOC::BCM283X, ECPU::ARM1176JZF_S },
		{ "Raspberry Pi Compute Module Rev 1.0",	0x20000000,	ESOC::BCM283X, ECPU::ARM1176JZF_S },
		{ "Raspberry Pi Zero Rev 1.3",				0x20000000,	ESOC::BCM283X, ECPU::ARM1176JZF_S },
		{ "Raspberry Pi Zero W Rev 1.1",			0x20000000,	ESOC::BCM283X, ECPU::ARM1176JZF_S },
		{ "Raspberry Pi Zero 2 W Rev 1.0",			0x3f000000,	ESOC::BCM283X, ECPU::CortexA53    },
		{ "Raspberry Pi 2 Model B Rev 1.0",			0x3f000000,	ESOC::BCM283X, ECPU::CortexA7     },
		{ "Raspberry Pi 2 Model B Rev 1.1",			0x3f000000,	ESOC::BCM283X, ECPU::CortexA7     },
		{ "Raspberry Pi 3 Model B Rev 1.2",			0x3f000000,	ESOC::BCM283X, ECPU::CortexA53    },
		{ "Raspberry Pi 3 Model B Plus Rev 1.3",	0x3f000000,	ESOC::BCM283X, ECPU::CortexA53    },
		{ "Raspberry Pi 3 Model A Plus Rev 1.0",	0x3f000000,	ESOC::BCM283X, ECPU::CortexA53    },
		{ "Raspberry Pi Compute Module 3",			0x3f000000,	ESOC::BCM283X, ECPU::CortexA53    },
		{ "Raspberry Pi Compute Module 3+",			0x3f000000,	ESOC::BCM283X, ECPU::CortexA53    },
		{ "Raspberry Pi 4 Model B Rev 1.1",			0xfe000000,	ESOC::BCM2711, ECPU::CortexA72    },
		{ "Raspberry Pi 4 Model B Rev 1.2",			0xfe000000,	ESOC::BCM2711, ECPU::CortexA72    },
		{ "Raspberry Pi 4 Model B Rev 1.4",			0xfe000000,	ESOC::BCM2711, ECPU::CortexA72    },
		{ "Raspberry Pi 400",						0xfe000000,	ESOC::BCM2711, ECPU::CortexA72    },
		{ "Raspberry Pi Compute Module 4",			0xfe000000,	ESOC::BCM2711, ECPU::CortexA72    },
		{ "Raspberry Pi 5 Model B",					0x7e000000,	ESOC::BCM2712, ECPU::CortexA76    },
		{ "Raspberry Pi Compute Module 5",			0x7e000000,	ESOC::BCM2712, ECPU::CortexA76    },
	};

	// from ChatGPT o4-mini-high:
	// const model_config_t MODEL_CONFIG[] = {
	// 	{ "Raspberry Pi Model B Rev 1",            0x20200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Model B Rev 2",            0x20200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Model A Rev 2",            0x20200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Model A Plus Rev 1.1",     0x20200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Model B Plus Rev 1.2",     0x20200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Compute Module Rev 1.0",   0x20200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Zero Rev 1.3",             0x20200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Zero W Rev 1.1",           0x20200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Zero 2 W Rev 1.0",         0x3f200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi 2 Model B Rev 1.1",        0x3f200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi 3 Model B Rev 1.2",        0x3f200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi 3 Model B Plus Rev 1.3",   0x3f200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi 3 Model A Plus Rev 1.1",   0x3f200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Compute Module 3",         0x3f200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi Compute Module 3 Plus",    0x3f200000, ESOC::BCM283X },
	// 	{ "Raspberry Pi 4 Model B Rev 1.1",        0xfe200000, ESOC::BCM2711 },
	// 	{ "Raspberry Pi 4 Model B Rev 1.2",        0xfe200000, ESOC::BCM2711 },
	// 	{ "Raspberry Pi 4 Model B Rev 1.4",        0xfe200000, ESOC::BCM2711 },
	// 	{ "Raspberry Pi 4 Model B Plus Rev 1.5",   0xfe200000, ESOC::BCM2711 },
	// 	{ "Raspberry Pi 400",                      0xfe200000, ESOC::BCM2711 },
	// 	{ "Raspberry Pi Compute Module 4",         0xfe200000, ESOC::BCM2711 },
	// 	{ "Raspberry Pi Compute Module 4S",        0xfe200000, ESOC::BCM2711 },
	// 	{ "Raspberry Pi 5 Model B Rev 1.0",        0x7e200000, ESOC::BCM2712 },
	// 	{ "Raspberry Pi 5 Model B Rev 1.1",        0x7e200000, ESOC::BCM2712 },
	// 	{ "Raspberry Pi Compute Module 5",         0x7e200000, ESOC::BCM2712 },
	// };

	static const u32_t GPIO_OFFSET = 0x200000;
	static const u32_t GPIO_SIZE = 0x1000;
	static const u32_t GPIO_FUNCTION_SELECT_REGISTER = 0x00;
	static const u32_t GPIO_SET_REGISTER              = 0x1C / 4;
	static const u32_t GPIO_CLEAR_REGISTER            = 0x28 / 4;
	static const u32_t GPIO_LEVEL_REGISTER            = 0x34 / 4;
	static const u32_t BCM283X_GPIO_PULL_CONFIG_REGISTER = 0x94 / 4;
	static const u32_t BCM283X_GPIO_PULL_CLOCK_REGISTER  = 0x98 / 4;
	static const u32_t BCM2711_GPIO_PULL_CONFIG_REGISTER = 0xE4 / 4;
	static const u32_t TIMER_OFFSET = 0x003000;
	static const u32_t TIMER_SIZE = 0x1000;

	static volatile u32_t* gpio = nullptr;
	static volatile u32_t* timer = nullptr;

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
		volatile u32_t& reg = gpio[GPIO_LEVEL_REGISTER + index / 32];
		return (reg & (1 << (index % 32))) != 0;
	}

	void TPin::State(const bool new_state)
	{
		volatile u32_t& reg = gpio[(new_state ? GPIO_SET_REGISTER : GPIO_CLEAR_REGISTER) + index / 32];
		reg = (1 << (index % 32));
	}

	EMode TPin::Mode() const
	{
		volatile u32_t& reg = gpio[GPIO_FUNCTION_SELECT_REGISTER + index / 10];
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
		volatile u32_t& reg = gpio[GPIO_FUNCTION_SELECT_REGISTER + index / 10];

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
		volatile u32_t& reg = gpio[GPIO_FUNCTION_SELECT_REGISTER + index / 10];

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
		volatile u32_t& reg = gpio[GPIO_FUNCTION_SELECT_REGISTER + index / 10];
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

	usys_t TPin::Index() const
	{
		return index;
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

	const model_config_t* DetectModel()
	{
		const char buffer[64] = {};
		TPath p = "/sys/firmware/devicetree/base/model";
		if(!p.IsFile())
			return nullptr;
		TFile model_file(p);
		model_file.Read((byte_t*)buffer, sizeof(buffer) - 1);

		for(unsigned i = 0; i < sizeof(MODEL_CONFIG)/sizeof(MODEL_CONFIG[0]); i++)
			if(strcmp(MODEL_CONFIG[i].name, buffer) == 0)
				return MODEL_CONFIG + i;

		return nullptr;
	}

	const model_config_t* DetectModelThrow()
	{
		const model_config_t* m = DetectModel();
		EL_ERROR(m == nullptr, TException, "Unknown Raspberry Pi Model. Please update MODEL_CONFIG array.");
		return m;
	}

	TBCM283X::TBCM283X() :
		claimed_pins(0),
		model(DetectModelThrow()),
		file("/dev/mem", TAccess::RW),
		gpio_map(&file, model->peri_base + GPIO_OFFSET, GPIO_SIZE),
		timer_map(&file, model->peri_base + TIMER_OFFSET, TIMER_SIZE)
	{
		gpio = (volatile u32_t*)&gpio_map[0];
		timer = (volatile u32_t*)&timer_map[0];
	}

	void TBCM283X::_PullResistor(const unsigned index, const EPull new_pull)
	{
		if(model->soc == ESOC::BCM283X)
		{
			switch(new_pull)
			{
				case EPull::DISABLED:
					gpio[BCM283X_GPIO_PULL_CONFIG_REGISTER] = 0b00;
					break;

				case EPull::UP:
					gpio[BCM283X_GPIO_PULL_CONFIG_REGISTER] = 0b10;
					break;

				case EPull::DOWN:
					gpio[BCM283X_GPIO_PULL_CONFIG_REGISTER] = 0b01;
					break;

				default:
					EL_THROW(TLogicException);
			}

			volatile u32_t& clock_reg = gpio[BCM283X_GPIO_PULL_CLOCK_REGISTER + index / 32];
			Delay(150);
			clock_reg = (1 << (index % 32));
			Delay(150);
			gpio[BCM283X_GPIO_PULL_CONFIG_REGISTER] = 0b00;
			clock_reg = 0;
		}
		else if(model->soc == ESOC::BCM2711)
		{
			volatile u32_t& reg = gpio[BCM2711_GPIO_PULL_CONFIG_REGISTER + index / 16];

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
		static auto instance = std::unique_ptr<TBCM283X>(new TBCM283X());
		return instance.get();
	}

	volatile u32_t* TBCM283X::_Set(const u8_t idx)
	{
		return gpio + GPIO_SET_REGISTER + idx;
	}

	volatile u32_t* TBCM283X::_Clear(const u8_t idx)
	{
		return gpio + GPIO_CLEAR_REGISTER + idx;
	}

	volatile const u32_t* TBCM283X::_Level(const u8_t idx) const
	{
		return gpio + GPIO_LEVEL_REGISTER + idx;
	}

	volatile const u32_t* TBCM283X::_GpioTimerLow() const
	{
		// CLO is at offset 0x0004
		return timer + 1;
	}

	volatile const u32_t* TBCM283X::_GpioTimerHigh() const
	{
		// CHI is at offset 0x0008
		return timer + 2;
	}

	TTime TBCM283X::GpioTime() const
	{
		volatile const u32_t* timer_low_reg = _GpioTimerLow();
		volatile const u32_t* timer_high_reg = _GpioTimerHigh();
		u32_t low, high1, high2;
		do
		{
			high1 = *timer_high_reg;
			low = *timer_low_reg;
			high2 = *timer_high_reg;
		}
		while(high1 != high2);
		u64_t microseconds = high1;
		microseconds <<= 32;
		microseconds |= low;
		const u64_t seconds = microseconds / 1000000ULL;
		microseconds -= seconds * 1000000ULL;
		const u64_t attoseconds = microseconds * 1000000000000ULL;
		return TTime((s64_t)seconds, (s64_t)attoseconds);
	}

	TBCM283X::~TBCM283X()
	{
	}
}

#endif
