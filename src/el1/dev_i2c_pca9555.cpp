#include "dev_i2c_pca9555.hpp"

// FIXME remove
#include <iostream>
#include <stdio.h>

// TODO: the code below blindly assumes a little endian system

namespace el1::dev::i2c::pca9555
{
	using namespace gpio;
	using namespace system::waitable;
	using namespace system::task;

	static constexpr u16_t MakeMask(const u8_t n_bits, const u8_t shift)
	{
		if(n_bits == 0)
			return 0;

		if(n_bits >= 16)
			return 0xffff;

		u16_t mask = 1 << n_bits;
		mask--;
		mask <<= shift;

		return mask;
	}

	static constexpr u16_t GetBits(const u16_t reg, const u8_t n_bits, const u8_t at)
	{
		const u16_t mask = MakeMask(n_bits, at);
		return (reg & mask) >> at;
	}

	static void SetBits(u16_t& reg, const u8_t n_bits, const u8_t at, u16_t value)
	{
		const u16_t mask = MakeMask(n_bits, at);
		value <<= at;
		value &= mask;

		u16_t tmp = reg & (~mask);
		tmp |= value;
		reg = tmp;
	}

	IController* TPin::Controller() const
	{
		return controller;
	}

	void TPin::AutoCommit(const bool nac)
	{
		this->auto_commit = nac;
	}

	bool TPin::State() const
	{
		if(this->Mode() == EMode::OUTPUT)
		{
			return GetBits(this->controller->reg_new.output, 1, this->index) != 0;
		}
		else
		{
			if(this->auto_commit)
				this->controller->Poll();

			switch(this->trigger)
			{
				case ETrigger::DISABLED:
					break;

				case ETrigger::RISING_EDGE:
					this->state_ref = 0x00;
					break;

				case ETrigger::FALLING_EDGE:
					this->state_ref = 0xff;
					break;

				case ETrigger::BOTH_EDGES:
					this->state_ref = this->controller->reg_ref.arr_input[this->index >= 8 ? 1 : 0];
					break;
			}

			return GetBits(this->controller->reg_ref.input, 1, this->index) != 0;
		}
	}

	void TPin::State(const bool new_state)
	{
		EL_ERROR(this->Mode() != EMode::OUTPUT, TException, "pin is not in OUTPUT mode");
		SetBits(this->controller->reg_new.output, 1, this->index, new_state ? 1 : 0);
		if(this->auto_commit)
			this->controller->Commit();
	}

	EMode TPin::Mode() const
	{
		return GetBits(this->controller->reg_new.config, 1, this->index) ? EMode::INPUT : EMode::OUTPUT;
	}

	void TPin::Mode(const EMode new_mode)
	{
		switch(new_mode)
		{
			case EMode::DISABLED:
				Trigger(ETrigger::DISABLED);
				// fall through

			case EMode::INPUT:
				SetBits(this->controller->reg_new.config, 1, this->index, 1);
				break;

			case EMode::OUTPUT:
				Trigger(ETrigger::DISABLED);
				SetBits(this->controller->reg_new.config, 1, this->index, 0);
				break;

			case EMode::ALT_FUNC:
				EL_THROW(TInvalidArgumentException, "new_mode", "ALT_FUNC can only be set through AlternateFunction(...) - and is not supported by PCA9555");
		}

		if(this->auto_commit)
			this->controller->Commit();
	}

	void TPin::AlternateFunction(const int)
	{
		// not supported
		EL_NOT_IMPLEMENTED;
	}

	int TPin::AlternateFunction() const
	{
		// not supported
		EL_NOT_IMPLEMENTED;
	}

	ETrigger TPin::Trigger() const
	{
		return this->trigger;
	}

	void TPin::Trigger(const ETrigger new_trigger)
	{
		EL_ERROR(new_trigger != ETrigger::DISABLED && this->Mode() != EMode::INPUT, TException, TString::Format("pin #%d is not in input mode and thus cannot be set as trigger", this->index));
		this->trigger = new_trigger;
	}

	EPull TPin::Pull() const
	{
		return EPull::UP;
	}

	void TPin::Pull(const EPull new_pull)
	{
		if(new_pull == EPull::UP)
			return;

		// not supported
		EL_NOT_IMPLEMENTED;
	}

	TMemoryWaitable<u8_t>& TPin::OnInputTrigger()
	{
		return this->waitable;
	}

	usys_t TPin::Index() const
	{
		return index;
	}

	TPin::TPin(TPCA9555* const controller, const u8_t index) : controller(controller),
		waitable(controller->reg_ref.arr_input + (index >= 8 ? 1 : 0), &this->state_ref, MakeMask(1, index % 8)),
		index(index), auto_commit(true), trigger(ETrigger::DISABLED), state_ref(0)
	{
		const u16_t mask = MakeMask(1, this->index);
		EL_ERROR((this->controller->claimed & mask) != 0, TException, TString::Format("pin #%d already claimed", this->index));
		this->controller->claimed |= mask;
	}

	TPin::~TPin()
	{
		AutoCommit(true);
		Mode(EMode::INPUT);
		this->controller->claimed &= (~MakeMask(1, this->index));
	}

	void TPCA9555::WriteRegister(const write_reg_t wr)
	{
		EL_ERROR(sizeof(wr) != 3, TLogicException);
		this->device->WriteAll((const byte_t*)&wr, 3);
	}

	void TPCA9555::IrqMain()
	{
		this->irq->AutoCommit(true);
		this->irq->Mode(EMode::INPUT);
		this->irq->Trigger(ETrigger::FALLING_EDGE);

		for(;;)
		{
			if(this->irq->State())
			{
// 				std::cerr<<"TPCA9555::IrqMain(): going to sleep\n";
				this->irq->OnInputTrigger().WaitFor();
			}

// 			std::cerr<<"TPCA9555::IrqMain(): received IRQ!\n";
			if(!this->irq->State())
			{
				this->Poll();
// 				std::cerr<<"TPCA9555::IrqMain(): updated input registers: "<<this->reg_ref.input<<"\n";
			}
			else
			{
// 				std::cerr<<"TPCA9555::IrqMain(): false IRQ\n";
			}
		}
	}

	TPCA9555::TPCA9555(std::unique_ptr<II2CDevice> device, std::unique_ptr<gpio::IPin> irq) : device(std::move(device)), irq(std::move(irq)), claimed(0)
	{
		WriteRegister({REG_CONFIG,   0xffff});
		WriteRegister({REG_POLARITY, 0x0000});
		WriteRegister({REG_OUTPUT,   0xffff});

		this->reg_ref.input    = this->device->ReadWordRegister(REG_INPUT);
		this->reg_ref.output   = this->device->ReadWordRegister(REG_OUTPUT);
		this->reg_ref.polarity = this->device->ReadWordRegister(REG_POLARITY);
		this->reg_ref.config   = this->device->ReadWordRegister(REG_CONFIG);
		this->reg_new = this->reg_ref;

		EL_ERROR(this->reg_ref.output   != 0xffff, TException, TString("unable to init PCA9555 at address %x", this->device->Address()));
		EL_ERROR(this->reg_ref.polarity != 0x0000, TException, TString("unable to init PCA9555 at address %x", this->device->Address()));
		EL_ERROR(this->reg_ref.config   != 0xffff, TException, TString("unable to init PCA9555 at address %x", this->device->Address()));

		if(this->irq)
		{
			this->fiber.Start(TFunction<void>(this, &TPCA9555::IrqMain));
		}
	}

	std::unique_ptr<gpio::IPin> TPCA9555::ClaimPin(const usys_t index)
	{
		return std::unique_ptr<gpio::IPin>(new TPin(this, index));
	}

	void TPCA9555::Commit()
	{
		if(this->irq == nullptr)
			Poll();

		if(this->reg_ref.config != this->reg_new.config)
		{
			WriteRegister({REG_CONFIG, this->reg_new.config});
			this->reg_ref.config = this->reg_new.config;
		}

		if(this->reg_ref.output != this->reg_new.output)
		{
			WriteRegister({REG_OUTPUT, this->reg_new.output});
			this->reg_ref.output = this->reg_new.output;
		}

		if(this->reg_ref.polarity != this->reg_new.polarity)
		{
			WriteRegister({REG_POLARITY, this->reg_new.polarity});
			this->reg_ref.polarity = this->reg_new.polarity;
		}
	}

	void TPCA9555::Poll()
	{
		this->reg_ref.input = this->device->ReadWordRegister(REG_INPUT);
		EL_ERROR((this->reg_ref.input & ~this->reg_ref.config) != (this->reg_ref.output & ~this->reg_ref.config), TLogicException);
	}
}
